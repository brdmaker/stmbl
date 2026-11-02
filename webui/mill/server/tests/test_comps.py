"""comps.py against a stand-in halcompile (MILLWEB_HALCOMPILE): compile, cache, install, remove, errors."""
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import comps  # noqa: E402

FAKE = os.path.join(os.path.dirname(os.path.dirname(HERE)), "test", "fake-halcompile")


class Comps(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        self.mdir = os.path.join(self.d, "machine")
        os.makedirs(os.path.join(self.mdir, "comp"))
        self.lib, self.stock = os.path.join(self.d, "modules"), os.path.join(self.d, "stock")
        os.makedirs(self.lib)
        os.makedirs(self.stock)
        for m in ("hal_lib", "and2"):  # LinuxCNC's own modules: links in the writable directory, as on the image
            open(os.path.join(self.stock, m + ".so"), "w").write("stock\n")
            os.symlink(os.path.join(self.stock, m + ".so"), os.path.join(self.lib, m + ".so"))
        os.environ.update(FAKE_DIR=self.d, MILLWEB_HALCOMPILE=FAKE, MILLWEB_RTLIB_DIR=self.lib, MILLWEB_RTLIB_STOCK=self.stock)
        self.logged = []

    def tearDown(self):
        for k in ("MILLWEB_HALCOMPILE", "MILLWEB_RTLIB_DIR", "MILLWEB_RTLIB_STOCK"):
            del os.environ[k]

    def comp(self, name, text):
        open(os.path.join(self.mdir, "comp", name + ".comp"), "w").write(text)

    def build(self):
        return comps.build(self.mdir, self.logged.append)

    def calls(self):
        try:
            return open(os.path.join(self.d, "halcompile-calls")).read().split()
        except OSError:
            return []

    def test_no_comps_nothing_to_do(self):
        self.assertIsNone(self.build())
        self.assertEqual(self.calls(), [])

    def test_rt_comp_compiled_installed_and_cached(self):
        self.comp("blink", "component blink;\npin out bit out;\n")
        self.assertEqual(self.build(), "Compiled comp: blink")
        self.assertEqual(open(os.path.join(self.lib, "blink.so")).read(), "built from component blink;\n")
        self.assertIsNone(self.build())                       # the next start: from the cache
        self.assertEqual(self.calls(), ["blink.comp"])
        self.comp("blink", "component blink; // changed\npin out bit out;\n")
        self.build()                                          # changed: compiled again
        self.assertEqual(len(self.calls()), 2)

    def test_an_update_of_linuxcnc_compiles_them_again(self):
        self.comp("blink", "component blink;\n")
        self.build()
        st = os.stat(os.path.join(self.stock, "hal_lib.so"))
        os.utime(os.path.join(self.stock, "hal_lib.so"), (st.st_atime, st.st_mtime + 100))
        self.build()
        self.assertEqual(len(self.calls()), 2)

    def test_userspace_comp_goes_on_the_path(self):
        self.comp("logger", "component logger;\noption userspace yes;\n")
        self.build()
        exe = os.path.join(comps.bin_dir(self.mdir), "logger")
        self.assertTrue(os.access(exe, os.X_OK))
        self.assertFalse(os.path.exists(os.path.join(self.lib, "logger.so")))

    def test_a_compile_error_says_where(self):
        self.comp("bad", "component bad;\nBROKEN\n")
        with self.assertRaises(comps.Error) as e:
            self.build()
        self.assertEqual(str(e.exception), "comp/bad.comp: bad.comp:3: error: expected ';' before '}' token")
        self.assertIn("Compiling realtime bad.c", "".join(self.logged))   # the whole output in the log
        self.assertFalse(os.listdir(os.path.join(self.mdir, "comp")) != ["bad.comp"])  # nothing written beside it

    def test_replacing_linuxcncs_own_and_taking_it_out_again(self):
        self.comp("and2", "component and2; // my own\n")
        self.assertIn("replacing LinuxCNC's own and2", self.build())
        self.assertFalse(os.path.islink(os.path.join(self.lib, "and2.so")))
        self.assertEqual(open(os.path.join(self.stock, "and2.so")).read(), "stock\n")  # the original untouched
        os.remove(os.path.join(self.mdir, "comp", "and2.comp"))
        self.build()
        self.assertEqual(os.readlink(os.path.join(self.lib, "and2.so")), os.path.join(self.stock, "and2.so"))

    def test_a_removed_comp_is_uninstalled(self):
        self.comp("blink", "component blink;\n")
        self.build()
        os.remove(os.path.join(self.mdir, "comp", "blink.comp"))
        self.build()
        self.assertFalse(os.path.exists(os.path.join(self.lib, "blink.so")))

    def test_a_modules_directory_that_isnt_writable_says_so(self):
        os.chmod(self.lib, 0o555)
        self.comp("blink", "component blink;\n")
        try:
            if os.access(self.lib, os.W_OK):
                self.skipTest("root writes anyway")
            with self.assertRaises(comps.Error) as e:
                self.build()
            self.assertIn("can't install it into", str(e.exception))
        finally:
            os.chmod(self.lib, 0o755)


if __name__ == "__main__":
    unittest.main()
