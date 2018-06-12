/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  Copyright 2010 Lennart Poettering
***/

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#if HAVE_SELINUX
#include <selinux/selinux.h>
#endif

#include "log.h"
#include "macro.h"
#include "selinux-setup.h"
#include "selinux-util.h"
#include "string-util.h"
#include "util.h"

#if HAVE_SELINUX
_printf_(2,3)
static int null_log(int type, const char *fmt, ...) {
        return 0;
}
#endif

int mac_selinux_setup(bool *loaded_policy) {

#if HAVE_SELINUX
        int enforce = 0;
        usec_t before_load, after_load;
        char *con;
        int r;
        static const union selinux_callback cb = {
                .func_log = null_log,
        };

        bool initialized = false;

        assert(loaded_policy);

        /* Turn off all of SELinux' own logging, we want to do that */
        selinux_set_callback(SELINUX_CB_LOG, cb);

        /* Don't load policy in the initrd if we don't appear to have
         * it.  For the real root, we check below if we've already
         * loaded policy, and return gracefully.
         */
        if (in_initrd() && access(selinux_path(), F_OK) < 0)
                return 0;

        /* Already initialized by somebody else? */
        r = getcon_raw(&con);
        if (r == 0) {
                initialized = !streq(con, "kernel");
                freecon(con);
        }

        /* Make sure we have no fds open while loading the policy and
         * transitioning */
        log_close();

        /* Now load the policy */
        before_load = now(CLOCK_MONOTONIC);
        r = selinux_init_load_policy(&enforce);
        if (r == 0) {
                _cleanup_(mac_selinux_freep) char *label = NULL;
                char timespan[FORMAT_TIMESPAN_MAX];

                mac_selinux_retest();

                /* Transition to the new context */
                r = mac_selinux_get_create_label_from_exe(SYSTEMD_BINARY_PATH, &label);
                if (r < 0 || !label) {
                        log_open();
                        log_error("Failed to compute init label, ignoring.");
                } else {
                        r = setcon_raw(label);

                        log_open();
                        if (r < 0)
                                log_error("Failed to transition into init label '%s', ignoring.", label);
                }

                after_load = now(CLOCK_MONOTONIC);

                log_info("Successfully loaded SELinux policy in %s.",
                         format_timespan(timespan, sizeof(timespan), after_load - before_load, 0));

                *loaded_policy = true;

        } else {
                log_open();

                if (enforce > 0) {
                        if (!initialized) {
                                log_emergency("Failed to load SELinux policy.");
                                return -EIO;
                        }

                        log_warning("Failed to load new SELinux policy. Continuing with old policy.");
                } else
                        log_debug("Unable to load SELinux policy. Ignoring.");
        }
#endif

        return 0;
}
