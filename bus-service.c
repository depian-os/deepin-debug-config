#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <systemd/sd-daemon.h>

#include "util.h"
#include "module_configure.h"

typedef uint64_t usec_t;
typedef bool (*check_idle_t)(void *userdata);

#define NO_EXIT_TIMEOUT UINT64_MAX

#define DEBUG_CONFIG_DBUS_NAME  "org.deepin.DebugConfig"
#define DEBUG_CONFIG_DBUS_INTERFACE  "org.deepin.DebugConfig"
#define DEBUG_CONFIG_DBUS_PATH  "/org/deepin/DebugConfig"

#define STAT_FILE "/proc/self/stat"
// #define ACTION_ID "com.deepin.daemon.accounts.user-administration"
#define ACTION_ID "org.deepin.DebugConfig.SetDebug"


#define TAKE_PTR(ptr)                           \
        ({                                      \
                typeof(ptr) _ptr_ = (ptr);      \
                (ptr) = NULL;                   \
                _ptr_;                          \
        })

typedef struct Context {
        sd_bus *bus;
        char *debug_level;
} Context;

typedef struct MethodResult {
        char *method_name;
        char *method_res;
} MethodResult;

//d-bus safe
void print_sd_bus_error(const sd_bus_error *error) {
    if (error) {
        if (sd_bus_error_has_name(error, NULL)) {
            fprintf(stderr, "D-Bus Error: %s\n", error->name);
        }
        if (error->message) {
            fprintf(stderr, "D-Bus Error Message: %s\n", error->message);
        }
    } else {
        fprintf(stderr, "No D-Bus error information available.\n");
    }
}

uint64_t get_process_starttime() {
    FILE *file;
    uint64_t starttime = -1;
    int field_number = 21;  // starttime is the 22nd field in /proc/self/stat

    file = fopen(STAT_FILE, "r");
    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    // Read through the file to reach the desired field
    for (int i = 0; i < field_number; i++) {
        if (fscanf(file, "%*s") == EOF) {  // Skip fields
            perror("fscanf");
            fclose(file);
            return -1;
        }
    }

    // Read the starttime field
    if (fscanf(file, "%ld", &starttime) != 1) {
        perror("fscanf");
        fclose(file);
        return -1;
    }

    fclose(file);
    return starttime;
}


int interactive_permission_check(sd_bus *bus, const char *action, const char **details, const char *caller) {  // pid_t mpid, uid_t muid, uint64_t mtime
    sd_bus_message *request = NULL;
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int challenge ;
    int authorized ;
    int r;

//     const char *unique_name;
//     uint64_t start_time_uint64 = mtime;
//     pid_t pid = mpid;
//     uid_t uid = muid;
//     int32_t uid_as_int32 = (int32_t)uid;
//     uint32_t pid_as_uint32 = (uint32_t)pid;

    assert(action);

    r = sd_bus_message_new_method_call(bus,
                                        &request,
                                        "org.freedesktop.PolicyKit1",
                                        "/org/freedesktop/PolicyKit1/Authority",
                                        "org.freedesktop.PolicyKit1.Authority",
                                        "CheckAuthorization");
    if (r < 0) {
        return r;
    }

//     r = sd_bus_get_unique_name(bus, &unique_name);
//     if (r < 0) {
//         fprintf(stderr, "Failed to get unique name: %s\n", strerror(-r));
//         return EXIT_FAILURE;
//     }
    // Append parameters to the method call
//     r = sd_bus_message_append(request,
//                               "(sa{sv})s",
//                               "unix-process", 3, "pid", "u", pid_as_uint32,"start-time", "t", start_time_uint64, "uid", "i", uid_as_int32,
//                               action);

    r = sd_bus_message_append(request,
                              "(sa{sv})s",
                              "system-bus-name", 1, "name", "s", caller,
                              action);

    if (r < 0) {
        return r;
    }

    r = sd_bus_message_append(request, "a{ss}", NULL);
    if (r < 0)
        return r;

    // Set up interactive authentication.
    r = sd_bus_message_append(request, "us", 1, NULL);
    if (r < 0) {
        return r;
    }

    // Send the request and wait for the reply
    r = sd_bus_call(bus, request, NO_EXIT_TIMEOUT, &error, &reply);
    if (r < 0) {
        print_sd_bus_error(&error);
        if (sd_bus_error_has_name(&error, SD_BUS_ERROR_SERVICE_UNKNOWN)) {
            sd_bus_error_free(&error);
            return -EACCES;  // PolicyKit service not available, access denied
        }
        return r;
    }
//     usleep(100 * 1000);//休眠0.1秒

    // Read the reply
    r = sd_bus_message_enter_container(reply, 'r', "bba{ss}");
    if (r < 0) {
        return r;
    }


    r = sd_bus_message_read(reply, "bb", &authorized, &challenge);
    if (r < 0) {
        return r;
    }

    if (authorized) {
        sd_bus_message_unref(reply);
        return 0;  // Authorized
    }

    if (challenge) {
        printf("Authorization challenge required. Please respond to the challenge.\n");
        // Handle interactive challenge
        // This part is highly application-specific.
    }

    sd_bus_message_unref(reply);
    return 1;  // Not authorized but a challenge is required
}


static void context_clear(Context *c) {
        assert(c);
        free(c->debug_level);
        sd_bus_flush_close_unref(c->bus);
        deinit_module_cfgs();
}

static int context_read_data(Context *c) {
        return init_module_cfgs(MODULES_DEBUG_CONFIG_PATH);
}

static int send_method_finish_signal(sd_bus *bus, void *userdata) {
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL;
        MethodResult *u = userdata;
        int r;

        assert(bus);
        assert(u);

        r = sd_bus_message_new_signal(
                        bus,
                        &m,
                        DEBUG_CONFIG_DBUS_PATH,
                        DEBUG_CONFIG_DBUS_INTERFACE,
                        "MethodFinished");
        if (r < 0)
                return r;

        r = sd_bus_message_append(m, "ss", u->method_name,u->method_res);
        if (r < 0)
                return r;

        return sd_bus_send(bus, m, NULL);
}

static int method_set_debug(sd_bus_message *m, void *userdata, sd_bus_error *error) {
        int r,reboot = 0,ret = 0;
        Context *c = userdata;
        MethodResult mr = {"SetDebug",NULL};
        const char *sender;
        const char *name, *level;

        /* Get the caller's sender */
        sender = sd_bus_message_get_sender(m);
        if (!sender)
                return -EBADMSG;

        // Perform authorization check
        r = interactive_permission_check(c->bus, ACTION_ID, NULL, sender);
        if (r < 0) {
                sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "Authorization failed: %s", strerror(-r));
                return r;
        } else if (r > 0) {
                sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "Authorization challenge required.");
                return r;
        }


        /* Read the parameters */
        r = sd_bus_message_enter_container(m, 'a', "(ss)");
        if (r < 0)
                return r;

        for (;;) {

                r = sd_bus_message_read(m, "(ss)", &name, &level);
                if (r < 0) {
                        sd_bus_error_setf(error, SD_BUS_ERROR_INVALID_ARGS, "Invalid arg");
                        break;
                }
                if (r == 0)
                        break;

                r = config_module_set_debug_level_by_module_name(name,level);
                if (r < 0) {
                        sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "error,ret=%d",r);
                        ret = r;
                }
                if (strcmp(name,"all")==0 && r >= 0) {
                        free(c->debug_level);
                        c->debug_level = strdup(level);
                        if (!c->debug_level) {
                                sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "Memory allocation failed");
                                return -ENOMEM;
                        }
                }
                if (reboot == 0)
                        config_module_get_property_reboot(name,&reboot);
                if (name && strcmp(name,"all")==0) break;
        }
        sd_bus_message_exit_container(m);

        config_module_check_log();

        if (ret < 0)
                mr.method_res = "fail";
        else {
                if (reboot)
                        mr.method_res = "reboot";
                else
                        mr.method_res = "success";
                sd_bus_reply_method_return(m, NULL);
        }
        return send_method_finish_signal(c->bus,&mr);
}

#if 0
static int method_get_name_pair(sd_bus_message *m, void *userdata, sd_bus_error *error) {
        int r;

        return sd_bus_reply_method_return(m, NULL);
}
#endif

static int method_install_dbg(sd_bus_message *m, void *userdata, sd_bus_error *error) {
        int r;

        const char *sender;
        Context *c = userdata;

        /* Get the caller's sender */
        sender = sd_bus_message_get_sender(m);
        if (!sender)
                return -EBADMSG;

        // Perform authorization check
        r = interactive_permission_check(c->bus, ACTION_ID, NULL, sender);
        if (r < 0) {
                sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "Authorization failed: %s", strerror(-r));
                return r;
        } else if (r > 0) {
                sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "Authorization challenge required.");
                return r;
        }

        if (!check_can_install_dbg())
                return sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "cann't install dbg");

        r = sd_bus_message_enter_container(m, 'a', "s");
        if (r < 0)
                return r;

        for (;;) {
                const char *name;

                r = sd_bus_message_read(m, "s", &name);
                if (r < 0)
                        return sd_bus_error_setf(error, SD_BUS_ERROR_INVALID_ARGS, "Invalid arg");
                if (r == 0)
                        break;

                r = config_module_install_dbgpkgs_internal(name);
                if (r < 0)
                        return sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "error,ret=%d",r);
        }
        sd_bus_message_exit_container(m);
        return sd_bus_reply_method_return(m, NULL);
}

static int method_set_coredump(sd_bus_message *m, void *userdata, sd_bus_error *error) {
        int r;
        int b;

        const char *sender;
        Context *c = userdata;

        /* Get the caller's sender */
        sender = sd_bus_message_get_sender(m);
        if (!sender)
                return -EBADMSG;

        // Perform authorization check
        r = interactive_permission_check(c->bus, ACTION_ID, NULL, sender);
        if (r < 0) {
                sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "Authorization failed: %s", strerror(-r));
                return r;
        } else if (r > 0) {
                sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "Authorization challenge required.");
                return r;
        }

        r = sd_bus_message_read(m, "b", &b);
        if (r < 0)
                return sd_bus_error_setf(error, SD_BUS_ERROR_INVALID_ARGS, "Invalid arg");

        r = config_system_coredump(b);
        if (r < 0)
                return sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "error,ret=%d",r);

        return sd_bus_reply_method_return(m, NULL);
}

static int method_get_state(sd_bus_message *m, void *userdata, sd_bus_error *error) {
        Context *c = userdata;
        int r;
        char *debug_level = NULL;

        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        _cleanup_strv_free_ char **modules = NULL;
        char **module;

        r = sd_bus_message_read_strv(m, &modules);
        if (r < 0)
                return r;

        r = sd_bus_message_new_method_return(m, &reply);
        if (r < 0)
                return r;
        r = sd_bus_message_open_container(reply, 'a', "s");
        if (r < 0)
                return r;

        for (module = modules; module && *module; module++) {
                r = config_module_get_debug_level_by_type(*module, &debug_level);
                if (r < 0)
                        return sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "error get module %s,ret=%d",*module,r);
                r = sd_bus_message_append(reply,"s",debug_level);

                if (strcmp(*module,"all")==0) {
                        // 将 debug_level 复制到 c->debug_level
                        free(c->debug_level);
                        c->debug_level = strdup(debug_level);
                        if (!c->debug_level) {
                                sd_bus_error_setf(error, SD_BUS_ERROR_FAILED, "Memory allocation failed");
                                return -ENOMEM;
                        }
                }

                free(debug_level);
                if (r < 0)
                        return r;
        }
        r = sd_bus_message_close_container(reply);
        if (r < 0)
                return r;

        return sd_bus_send(NULL, reply, NULL);
}

static int property_debug_level(sd_bus *bus, const char *path, const char *interface, const char *property, sd_bus_message *reply, void *userdata, sd_bus_error *error) {
        Context *c = userdata;
        return sd_bus_message_append(reply, "s", c->debug_level ? c->debug_level : "");
}

/* The vtable of our little object, implements the net.poettering.Calculator interface */
static const sd_bus_vtable debug_config_vtable[] = {
        SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD("SetDebug", "a(ss)", NULL, method_set_debug, SD_BUS_VTABLE_UNPRIVILEGED),
        //SD_BUS_METHOD("GetNamePair", NULL, "a(ss)", method_get_name_pair,   SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("InstallDbg", "as", NULL, method_install_dbg,   SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("SetCoredump", "b", NULL, method_set_coredump,   SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("GetState", "as", "as", method_get_state,   SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_PROPERTY("AllDebugLevel", "s", property_debug_level, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),

        SD_BUS_SIGNAL("MethodFinished", "ss", 0),
        SD_BUS_VTABLE_END
};

static int connect_bus(Context *c) {
        int r;

        assert(c);

        r = sd_bus_default_system(&c->bus);
        //r = sd_bus_open_system(&bus);
        if (r < 0) {
                fprintf(stderr, "Failed to get default system: %s\n", strerror(-r));
                return r;
        }

        r = sd_bus_add_object_vtable(c->bus, NULL, DEBUG_CONFIG_DBUS_PATH, DEBUG_CONFIG_DBUS_INTERFACE, debug_config_vtable, c);
        if (r < 0) {
                fprintf(stderr, "Failed to add_object_vtable: %s\n", strerror(-r));
                return r;
        }

        r = sd_bus_request_name_async(c->bus, NULL, DEBUG_CONFIG_DBUS_NAME, 0, NULL, NULL);
        //r = sd_bus_request_name(c->bus, DEBUG_CONFIG_DBUS_NAME, 0);
        if (r < 0) {
                fprintf(stderr, "Failed to request_name_async: %s\n", strerror(-r));
                return r;
        }

        return 0;
}

int main(int argc, char *argv[]) {
        _cleanup_(context_clear) Context context = {};
        int r;
        bool exiting = false;

        umask(0022);

        if (argc != 1) {
                //log_error("This program takes no arguments.");
                return -EINVAL;
        }

        r = connect_bus(&context);
        if (r < 0) {
                fprintf(stderr, "Failed to connect_bus: %s\n", strerror(-r));
                return -EINVAL;
        }

        r = context_read_data(&context);
        if (r < 0) {
                fprintf(stderr, "Failed to context_read_data: %s\n", strerror(-r));
                return -EINVAL;
        }

        for (;;) {
                /* Process requests */
                r = sd_bus_process(context.bus, NULL);
                if (r < 0) {
                        fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
                        break;
                }
                if (r > 0) /* we processed a request, try to process another one, right-away */
                        continue;

                if (exiting)
                        break;

                /* Wait for the next request to process */
                r = sd_bus_wait(context.bus, NO_EXIT_TIMEOUT);

                if (r < 0) {
                        fprintf(stderr, "Failed to wait on bus: %s\n", strerror(-r));
                        break;
                }
                if (r == 0) {
                        r = sd_bus_try_close(context.bus);
                        if (r == -EBUSY)
                                continue;

                        /* Fallback for dbus1 connections: we
                         * unregister the name and wait for the
                         * response to come through for it */
                        if (r == -EOPNOTSUPP) {

                                /* Inform the service manager that we
                                 * are going down, so that it will
                                 * queue all further start requests,
                                 * instead of assuming we are already
                                 * running. */
                                sd_notify(false, "STOPPING=1");

                                r = sd_bus_release_name_async(context.bus, NULL, DEBUG_CONFIG_DBUS_NAME, NULL, NULL);
                                if (r < 0)
                                        break;

                                exiting = true;
                                continue;
                        }
                        break;
                }
        }

        return 0;
}

