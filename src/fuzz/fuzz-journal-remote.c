/* SPDX-License-Identifier: LGPL-2.1+ */

#include "fuzz.h"

#include <sys/mman.h>

#include "sd-journal.h"

#include "fd-util.h"
#include "fileio.h"
#include "fs-util.h"
#include "journal-remote.h"
#include "logs-show.h"
#include "memfd-util.h"
#include "strv.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
        RemoteServer s = {};
        char name[] = "/tmp/fuzz-journal-remote.XXXXXX.journal";
        void *mem;
        int fdin; /* will be closed by journal_remote handler after EOF */
        _cleanup_close_ int fdout = -1;
        sd_journal *j;
        int r;

        if (size <= 2)
                return 0;

        assert_se((fdin = memfd_new_and_map("fuzz-journal-remote", size, &mem)) >= 0);
        memcpy(mem, data, size);
        assert_se(munmap(mem, size) == 0);

        fdout = mkostemps(name, STRLEN(".journal"), O_CLOEXEC);
        assert_se(fdout >= 0);

        /* In */

        assert_se(journal_remote_server_init(&s, name, JOURNAL_WRITE_SPLIT_NONE, false, false) >= 0);

        assert_se(journal_remote_add_source(&s, fdin, (char*) "fuzz-data", false) > 0);

        while (s.active) {
                r = journal_remote_handle_raw_source(NULL, fdin, 0, &s);
                assert_se(r >= 0);
        }

        journal_remote_server_destroy(&s);
        assert_se(close(fdin) < 0 && errno == EBADF); /* Check that the fd is closed already */

        /* Out */

        r = sd_journal_open_files(&j, (const char**) STRV_MAKE(name), 0);
        assert_se(r >= 0);

        r = show_journal(stdout, j, OUTPUT_VERBOSE, 0, 0, -1, 0, NULL);
        assert_se(r >= 0);

        sd_journal_close(j);
        unlink(name);

        return 0;
}
