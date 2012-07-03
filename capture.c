/*
 *  2012 (C) Linaro 
 *  Sangwook Lee <sangwook.lee@linaro.org>
 *  FIMC V4L2 video capture example based on v4l2 capture.c 
 *
 *  This program can be used and distributed without restrictions.
 *  GPLv2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#include <common.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

struct buffer {
        void *                  start;
        size_t                  length;
};

static char *           dev_name        = NULL;
static io_method	io		= IO_METHOD_READ;
static int              fd              = -1;
struct buffer *         buffers         = NULL;
static unsigned int     n_buffers       = 0;



int fimc_sfmt(struct instance *i, int width, int height,
	 enum v4l2_buf_type type, unsigned long pix_fmt, int num_planes,
	 struct v4l2_plane_pix_format planes[])
{
	struct v4l2_format fmt;
	int ret;
	int n;

	memzero(fmt);
	fmt.fmt.pix_mp.pixelformat = pix_fmt;
	fmt.type = type;
	fmt.fmt.pix_mp.width = width;
	fmt.fmt.pix_mp.height = height;
	fmt.fmt.pix_mp.num_planes = num_planes;

	for (n = 0; n < num_planes; n++)
		memcpy(&fmt.fmt.pix_mp.plane_fmt[n], &planes[n],
			sizeof(*planes));

	fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;

	ret = ioctl(i->fimc.fd, VIDIOC_S_FMT, &fmt);

	if (ret != 0) {
		err("Failed to SFMT on %s of FIMC",
			dbg_type[type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE]);
		return -1;
	}

	if (fmt.fmt.pix_mp.width != width ||
		fmt.fmt.pix_mp.height != height ||
		fmt.fmt.pix_mp.num_planes != num_planes ||
		fmt.fmt.pix_mp.pixelformat != pix_fmt) {
		err("Format was changed by FIMC so we abort operations");
		return -1;
	}


	dbg("Successful SFMT on %s of FIMC (%dx%d)",
			dbg_type[type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE],
			width, height);

	return 0;
}


#define BUFFER_COUNT 4 

int fimc_setup_output(struct instance *i)
{
	struct v4l2_plane_pix_format planes[MFC_CAP_PLANES];
	struct v4l2_requestbuffers reqbuf;
	int ret;
	int n;

	i->mfc.cap_w		= 640; 
	i->mfc.cap_h		= 480;
	i->mfc.cap_buf_cnt	= BUFFER_COUNT;
/*
	for (n = 0; n < MFC_CAP_PLANES; n++) {
		planes[n].sizeimage = i->mfc.cap_buf_size[n];
		planes[n].bytesperline = i->mfc.cap_w;
	}

*/
	ret = fimc_sfmt(i, i->mfc.cap_w, i->mfc.cap_h,
		V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_PIX_FMT_YUYV,
		MFC_CAP_PLANES, planes);

	if (ret)
		return ret;

	memzero(reqbuf);
	reqbuf.count = i->mfc.cap_buf_cnt;
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	reqbuf.memory = V4L2_MEMORY_USERPTR;

	ret = ioctl(i->fimc.fd, VIDIOC_REQBUFS, &reqbuf);
	if (ret) {
		err("REQBUFS failed on OUTPUT of FIMC");
		return -1;
	}

	dbg("Succesfully setup OUTPUT of FIMC");

	return 0;
}

int fimc_open(struct instance *i, char *name)
{
	struct v4l2_capability cap;
	int ret;

	i->fimc.fd = open(name, O_RDWR, 0);
	if (i->fimc.fd < 0) {
		err("Failed to open FIMC: %s", name);
		return -1;
	}

	memzero(cap);
	ret = ioctl(i->fimc.fd, VIDIOC_QUERYCAP, &cap);
	if (ret != 0) {
		err("Failed to verify capabilities");
		return -1;
	}

	dbg("FIMC Info (%s): driver=\"%s\" bus_info=\"%s\" card=\"%s\" fd=0x%x",
			name, cap.driver, cap.bus_info, cap.card, i->fimc.fd);

	if (	!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) ||
		!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE) ||
		!(cap.capabilities & V4L2_CAP_STREAMING)) {
		err("Insufficient capabilities of FIMC device (is %s correct?)",
									name);
		return -1;
	}

        return 0;
}


static void errno_exit (const char *s)
{
	fprintf (stderr, "%s error %d, %s\n",
			s, errno, strerror (errno));

	exit (EXIT_FAILURE);
}

static int xioctl                          (int                    fd,
                                 int                    request,
                                 void *                 arg)
{
	int r;

	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}

int fimc_dec_dequeue_buf(struct instance *i, int *n, int nplanes, int type)
{
	struct v4l2_buffer buf;
	struct v4l2_plane planes[MFC_MAX_PLANES];
	int ret;

	memzero(buf);
	buf.type = type;
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.m.planes = planes;
	buf.length = nplanes;

	ret = ioctl(i->fimc.fd, VIDIOC_DQBUF, &buf);

	if (ret) {
		err("Failed to dequeue buffer");
		return -1;
	}

	*n = buf.index;

	dbg("Dequeued buffer with index %d on %s queue", buf.index,
		dbg_type[type==V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE]);

	return 0;
}

int fimc_dec_dequeue_buf_cap(struct instance *i, int *n)
{
	return fimc_dec_dequeue_buf(i, n, 1, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
}


static void read_frame(struct instance *i)
{
	int tmp;

	if (fimc_dec_dequeue_buf_cap(i, &tmp)) {
			i->error = 1;
			break;
		}
}


static void mainloop (struct instance *i)
{
	unsigned int count;

        count = 100;

        while (count-- > 0) {
                for (;;) {
                        fd_set fds;
                        struct timeval tv;
                        int r;

                        FD_ZERO (&fds);
                        FD_SET (i->fimc.fd, &fds);

                        /* Timeout. */
                        tv.tv_sec = 2;
                        tv.tv_usec = 0;

                        r = select (i->fimc.fd + 1, &fds, NULL, NULL, &tv);

                        if (-1 == r) {
                                if (EINTR == errno)
                                        continue;

                                errno_exit ("select");
                        }

                        if (0 == r) {
                                fprintf (stderr, "select timeout\n");
                                exit (EXIT_FAILURE);
                        }

			if (read_frame())
                    		break;
	
			/* EAGAIN - continue select loop. */
                }
        }
}


static void
uninit_device                   (void)
{
        unsigned int i;

	switch (io) {
	case IO_METHOD_READ:
		free (buffers[0].start);
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < n_buffers; ++i)
			if (-1 == munmap (buffers[i].start, buffers[i].length))
				errno_exit ("munmap");
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < n_buffers; ++i)
			free (buffers[i].start);
		break;
	}

	free (buffers);
}


static void init_userp	(unsigned int buffer_size)
{
	struct v4l2_requestbuffers req;
	unsigned int page_size;

	page_size = getpagesize ();
	buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

	CLEAR (req);

	req.count               = 4;
	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory              = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s does not support "
					"user pointer i/o\n", dev_name);
			exit (EXIT_FAILURE);
		} else {
			errno_exit ("VIDIOC_REQBUFS");
		}
	}

	buffers = calloc (4, sizeof (*buffers));

	if (!buffers) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
		buffers[n_buffers].length = buffer_size;
		buffers[n_buffers].start = memalign (/* boundary */ page_size,
				buffer_size);

		if (!buffers[n_buffers].start) {
			fprintf (stderr, "Out of memory\n");
			exit (EXIT_FAILURE);
		}
	}
}


static void
close_device                    (void)
{
        if (-1 == close (fd))
	        errno_exit ("close");

        fd = -1;
}


int fimc_stream(struct instance *i, enum v4l2_buf_type type, int status)
{
	int ret;

	ret = ioctl(i->fimc.fd, status, &type);
	if (ret) {
		err("Failed to change streaming on FIMC (type=%s, status=%s)",
			dbg_type[type==V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE],
			dbg_status[status==VIDIOC_STREAMOFF]);
		return -1;
	}

#if 0
	dbg("Stream %s on %s queue\n", dbg_status[status==VIDIOC_STREAMOFF],
		dbg_type[type==V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE]);
#endif

	return 0;
}


static init_userp_buffer(void)
{
	unsigned int page_size;
        page_size = getpagesize ();
        buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

	buffers = calloc (BUFFER_COUNT, sizeof (*buffers));

	if (!buffers) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < BUFFER_COUNT; ++n_buffers) {
		buffers[n_buffers].length = buffer_size;
		buffers[n_buffers].start = memalign (/* boundary */ page_size,
				buffer_size);

		if (!buffers[n_buffers].start) {
			fprintf (stderr, "Out of memory\n");
			exit (EXIT_FAILURE);
		}
	}
}

static void start_capturing (void) 
{
	unsigned int i;
	enum v4l2_buf_type type;
	struct v4l2_plane planes[MFC_CAP_PLANES];

	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;
		CLEAR (buf);
		buf.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		buf.memory      = V4L2_MEMORY_USERPTR;
		buf.index       = i;
		buf.m.planes = planes;
		buf.length = MFC_CAP_PLANES;

		buf.m.planes[0].bytesused = i->mfc.cap_buf_size[0];
		buf.m.planes[0].length = i->mfc.cap_buf_size[0];
		buf.m.planes[0].m.userptr = (unsigned long)i->mfc.cap_buf_addr[n][0];

		buf.m.planes[1].bytesused = i->mfc.cap_buf_size[1];
		buf.m.planes[1].length = i->mfc.cap_buf_size[1];
		buf.m.planes[1].m.userptr = (unsigned long)i->mfc.cap_buf_addr[n][1];

		buf.m.userptr   = (unsigned long) buffers[i].start;
		buf.length      = buffers[i].length;

		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
			errno_exit ("VIDIOC_QBUF");
	}

#if 0
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
		errno_exit ("VIDIOC_STREAMON");
#endif
	fimc_stream(&inst, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, VIDIOC_STREAMOFF);

}


/* 
 * In case of V4L2_MEMORY_USERPTR 
 * Caling order of ioctls commands
 * VIDIOC_QUERYCAP
 * VIDIOC_CROPCAP
 * VIDIOC_S_FMT
 * VIDIOC_REQBUFS
 * VIDIOC_QBUF
 * VIDIOC_STREAMON
 * VIDIOC_DQBUF
 * VIDIOC_QBUF (?)
 * VIDIOC_STREAMOFF
 */

int main (int   argc,  char **   argv)
{
	dev_name = "/dev/video1";
	struct instance inst;

	fimc_open(&inst, dev_name);
	fimc_setup_output(&inst);

	init_userp_buffer();
	start_capturing(); 

	mainloop (&inst);


	fimc_close(&inst);

	return 0;
}
