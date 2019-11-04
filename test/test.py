import ctypes
test = ctypes.CDLL("./libtest.so")
test.main()
