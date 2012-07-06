/*
 *  2012      (C) Linaro 
 *  Sangwook Lee <sangwook.lee@linaro.org>
 *  Only MMAP FIMC V4L2 video capture example based on v4l2 capture.c 
 * 
 *  Copyright (C) 2004 Samsung Electronics 
 *                   <SW.LEE, hitchcar@sec.samsung.com> 
 *		based yuv_4p.c 
 *
 * How to use this app(Ubuntu)
 *
 * $>  display -size 640x480 422X0.yuv
 *
 *  The main purpose of this app is to show to use V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
 *
 *  This program can be used and distributed without restrictions.
 *  GPLv2
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>		/* getopt_long() */
#include <fcntl.h>		/* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>		/* for videodev2.h */

#include <linux/videodev2.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#define PR_IO(x)	printf("v4l2-app:line%d "#x"\n",__LINE__)

typedef struct buffer {
	char		*addr[VIDEO_MAX_PLANES];
	unsigned long	size[VIDEO_MAX_PLANES];
	unsigned int	num_planes;
	unsigned int	index;
	//enum format	  fmt;
		struct v4l2_format fmt;
	unsigned int	width;
	unsigned int	height;
} fimc_buf_t;

fimc_buf_t *buffers = NULL;
static unsigned int n_buffers = 4;
static int capture_pix_width = 640;
static int capture_pix_height = 480;
static char * dev_name	= "/dev/video1";
static int g_file_desc = -1;
static int file_loop = 0;	/* change file name */
#define INPUT_DATA_COUNT 10 /* number of save files */

static void errno_exit(const char *s)
{
	fprintf (stderr, "%s error %d, %s\n",
			s, errno, strerror (errno));
	exit (EXIT_FAILURE);
}

static int xioctl (int fd, int request, void * arg)
{
	int r;

	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}

static int init_mmap(unsigned int *n_buffers)
{
	struct v4l2_requestbuffers req;
	unsigned int buf_index;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];

	memset(&req, 0, sizeof(req));
	req.count	= *n_buffers;
	req.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req.memory	= V4L2_MEMORY_MMAP;

	PR_IO(VIDIOC_REQBUFS);
	if (-1 == ioctl (g_file_desc, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno)
			printf("REQBUFS failed. No support for memory mapping?\n");
		else
			perror("VIDIOC_REQBUFS ioctl");
		return -1;
	}
	if (req.count < 2) {
		printf("Insufficient buffer memory\n");
		return -1;
	}

	/* Number of buffers might got adjusted by driver so we propagate real
	   value up to the caller */

	*n_buffers = req.count;

	buffers = calloc (req.count, sizeof (*buffers));

	if (!buffers) {
		printf("Out of memory\n");
		exit (EXIT_FAILURE);
	}

	PR_IO(VIDIOC_QUERYBUF);
	for (buf_index = 0; buf_index < req.count; ++buf_index) {
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));
		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.m.planes	= planes;
		buf.length	= 1; /* just one plane, depends on pixel format */
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= buf_index;

		if (-1 == ioctl (g_file_desc, VIDIOC_QUERYBUF, &buf)) {
			perror ("VIDIOC_QUERYBUF");
			return -1;
		}
		printf("====== buf_index %d ==================================\n", buf_index);
		printf("Plane offset: %d\n", buf.m.planes[0].m.mem_offset);
		printf("Plane length: %d\n", buf.m.planes[0].length);

		buffers[buf_index].size[0] = buf.m.planes[0].length;
		buffers[buf_index].addr[0] =
				mmap (NULL /* start anywhere */,
				buf.m.planes[0].length,
				PROT_READ | PROT_WRITE /* required */,
				MAP_SHARED /* recommended */,
				g_file_desc, buf.m.planes[0].m.mem_offset);

		if (MAP_FAILED == buffers[buf_index].addr[0]) {
			perror("mmap");
			return -1;
		}

		buffers[buf_index].index  = buf_index;
		buffers[buf_index].width  = capture_pix_width;
		buffers[buf_index].height = capture_pix_height;

		printf("mmaped: buf_index: %d, size: %ld, addr: %p\n", buf_index,
			buffers[buf_index].size[0], buffers[buf_index].addr[0]);
	}

	return 0;
}

static void init_v4l2_device (void) 
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	if (-1 == xioctl (g_file_desc, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s is no V4L2 device\n",
					dev_name);
			exit (EXIT_FAILURE);
		} else {
			errno_exit ("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
		fprintf (stderr, "%s is no video capture device\n",
				dev_name);
		exit (EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf (stderr, "%s does not support streaming i/o\n",
				dev_name);
		exit (EXIT_FAILURE);
	}

	CLEAR (cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	PR_IO(VIDIOC_CROPCAP);
	if (0 == xioctl (g_file_desc, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl (g_file_desc, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
				case EINVAL:
					/* Cropping not supported. */
					break;
				default:
					/* Errors ignored. */
					break;
			}
		}
	} else {
		/* Errors ignored. */
	}

	CLEAR (fmt);

	fmt.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix.width	= capture_pix_width;
	fmt.fmt.pix.height	= capture_pix_height;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field	= V4L2_FIELD_INTERLACED;

	PR_IO(VIDIOC_S_FMT);
	if (-1 == xioctl (g_file_desc, VIDIOC_S_FMT, &fmt))
		errno_exit ("VIDIOC_S_FMT");

	/* Note VIDIOC_S_FMT may change width and height. */

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

}


static void open_device (void)
{
	struct stat st;

	if (-1 == stat (dev_name, &st)) {
		fprintf (stderr, "Cannot identify '%s': %d, %s\n",
				dev_name, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	if (!S_ISCHR (st.st_mode)) {
		fprintf (stderr, "%s is no device\n", dev_name);
		exit (EXIT_FAILURE);
	}

	g_file_desc = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

	if (-1 == g_file_desc) {
		fprintf (stderr, "Cannot open '%s': %d, %s\n",
				dev_name, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}
}

static void save_yuv(int bpp, char *g_yuv)
{
	FILE *yuv_fp = NULL;
	char file_name[100];

	if (bpp == 16 ) {
		sprintf(&file_name[0], "422X%d.yuv", file_loop);
		printf("422X%d.yuv", file_loop);
	}
	else { 
		sprintf(&file_name[0], "420X%d.yuv", file_loop);
		printf("420X%d.yuv\n", file_loop);
	}
	fflush(stdout);
	/* file create/open, note to "wb" */
	yuv_fp = fopen(&file_name[0], "wb");
	if (!yuv_fp) {
		perror(&file_name[0]);
	}
	fwrite(g_yuv, 1, capture_pix_height * capture_pix_width * bpp / 8, yuv_fp);
	fclose(yuv_fp);
}

static void process_image (char *p)
{
	if (!file_loop) 
		return;	/* Discard first frame */ 
	save_yuv(16, p);
	file_loop++;
}

static int read_frame(void)
{
	struct v4l2_buffer buf;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	int index;

	CLEAR (buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;

	PR_IO(VIDIOC_DQBUF);
	if (-1 == xioctl (g_file_desc, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit ("VIDIOC_DQBUF");
		}
	}

	assert (buf.index < n_buffers);
	process_image (buffers[buf.index].addr[0]);

	index = buf.index;

	PR_IO(VIDIOC_QBUF);
	CLEAR (buf);
	buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory      = V4L2_MEMORY_MMAP;
	buf.index       = index;
	buf.m.planes    = planes;
	buf.length      = 1;
	buf.m.planes[0].bytesused = buffers[index].size[0];

	if (-1 == xioctl (g_file_desc, VIDIOC_QBUF, &buf))
		errno_exit ("VIDIOC_QBUF");

	return 1;
}

#define WAIT_DELAY 4
static void mainloop (void)
{
	unsigned int count = INPUT_DATA_COUNT;

	while (count-- > 0) {
		for (;;) {
			fd_set fds;
			struct timeval tv;
			int r;

			FD_ZERO (&fds);
			FD_SET (g_file_desc, &fds);

			/* Timeout. */
			tv.tv_sec = WAIT_DELAY;
			tv.tv_usec = 0;
			r = select (g_file_desc + 1, &fds, NULL, NULL, &tv);
			if (-1 == r) {
				if (EINTR == errno)
					continue;

				errno_exit ("select");
			}

			if (0 == r) {
				fprintf (stderr, "=====================\n");
				fprintf (stderr, "select timeout %d sec\n", (int)tv.tv_sec);
				fprintf (stderr, "=====================\n\n");
				exit (EXIT_FAILURE);
			}

			if (read_frame ())
				break;
			/* EAGAIN - continue select loop. */
		}
	}
}

void start_capturing(fimc_buf_t *bufs)
{
	unsigned int i;
	enum v4l2_buf_type type;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];

	PR_IO(VIDIOC_QBUF);
	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;
		CLEAR (buf);
		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= i;
		buf.m.planes	= planes;
		buf.length	= 1;
		buf.m.planes[0].bytesused = bufs[i].size[0];

		if (-1 == xioctl (g_file_desc, VIDIOC_QBUF, &buf))
			errno_exit ("VIDIOC_QBUF");
	}

	PR_IO(VIDIOC_STREAMON);
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (-1 == xioctl (g_file_desc, VIDIOC_STREAMON, &type))
		errno_exit ("VIDIOC_STREAMON");
}

static void stop_capturing(void)
{
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	if (-1 == xioctl (g_file_desc, VIDIOC_STREAMOFF, &type))
		errno_exit ("VIDIOC_STREAMOFF");
}

static void close_mmap(void)
{
	unsigned int i;
	for (i = 0; i < n_buffers; ++i)
		if (-1 == munmap (buffers[i].addr[0], buffers[i].size[0]))
			errno_exit ("munmap");
}

static void close_device (void)
{
	if (-1 == close (g_file_desc))
		errno_exit ("close");

	g_file_desc = -1;
}

/* 
 * In case of V4L2_MEMORY_USERPTR/MMAP 
 * Caling order of ioctls commands
 * VIDIOC_QUERYCAP
 * VIDIOC_CROPCAP
 * VIDIOC_S_FMT(V4L2_PIX_FMT_YUYV width/height)
 * VIDIOC_REQBUFS
 * VIDIOC_QBUF
 * VIDIOC_STREAMON
 * VIDIOC_DQBUF
 * VIDIOC_QBUF (?)
 * VIDIOC_STREAMOFF
 */

int main (int argc, char ** argv)
{
	int ret = 0;

	open_device();
	init_v4l2_device();

	ret = init_mmap(&n_buffers);
	if(ret)	 
		errno_exit ("init_mmap error !!!");

	start_capturing(buffers);
	mainloop();
	stop_capturing();
	close_mmap();
	close_device();
	exit(EXIT_SUCCESS);

	return ret;
}
