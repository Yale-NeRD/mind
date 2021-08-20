import random

ALLOC_SIZE_ONE_PAGE = 4096
ALLOC_SIZE_LARGE = 9999 * 4096
TRACE_LEN = 500000


def genTrace(alloc_size, trace_len, align, tid, r2w_ratio):
	a = ("uni" if alloc_size == ALLOC_SIZE_ONE_PAGE else "multi") + "Page"
	b = "align" + str(align)
	c = "tid" + str(tid)
	d = "r2w" + str(r2w_ratio)
	fileName = a + "_" + b + "_" + c + "_" + d

	f = open(fileName, "a")

	f.write(str(trace_len) + "\n")
	for i in range(trace_len):
		accessType = "r" if (random.randint(1, 100) <= r2w_ratio * 100) else "w"
		addr = str(random.randint(0, alloc_size - 1) // align * align + tid)
		val = "0" if accessType == "r" else str(random.randint(0, 127))
		f.write(accessType + " " + addr + " " + val + "\n")
	f.close()


#single thread, one page, different read to write ratio
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 2, 0, 0.5)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 2, 1, 0.5)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 2, 0, 0.01)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 2, 1, 0.01)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 2, 0, 0.99)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 2, 1, 0.99)

#single thread, multiple pages, different read to write ratio
# genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 0, 0.5)
# genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 1, 0.5)
# genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 0, 0.01)
# genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 1, 0.01)
# genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 0, 0.99)
# genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 1, 0.99)

#double thread, one page, different read to write ratio
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 4, 0, 0.5)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 4, 1, 0.5)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 4, 2, 0.5)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 4, 3, 0.5)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 4, 0, 0.01)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 4, 1, 0.01)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 4, 2, 0.01)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 4, 3, 0.01)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 4, 0, 0.99)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 4, 1, 0.99)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 4, 2, 0.99)
genTrace(ALLOC_SIZE_ONE_PAGE, TRACE_LEN, 4, 3, 0.99)

#double thread, mutiple pages, different read to write ratio
genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 0, 0.5)
genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 1, 0.5)
genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 2, 0.5)
genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 3, 0.5)
genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 0, 0.01)
genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 1, 0.01)
genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 2, 0.01)
genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 3, 0.01)
genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 0, 0.99)
genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 1, 0.99)
genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 2, 0.99)
genTrace(ALLOC_SIZE_LARGE, TRACE_LEN, 1024, 3, 0.99)
