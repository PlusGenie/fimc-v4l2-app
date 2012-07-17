/*
 *  Copyright (C) 2012 Linaro 
 *  Sangwook Lee <sangwook.lee@linaro.org>
 *   Only MMAP FIMC V4L2 video capture example
 *   based on V4L2 Specification, Appendix B: Video Capture Example 
 *   (http://v4l2spec.bytesex.org/spec/capture-example.html)   
 *   Merged v4l2grab 
 *
 *  Copyright (C) 2009 by Tobias Müller     
 *   Tobias_Mueller@twam.info               
 *
 *  Copyright (C) 2004 Samsung Electronics 
 *                   <SW.LEE, hitchcar@sec.samsung.com> 
 *		based yuv_4p.c 
 *
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

#include "jpeglib.h"

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
static int g_pix_width = 0;
static int g_pix_height = 0;
static char * dev_name	= "/dev/video1";
static int g_file_desc = -1;
static int file_loop = 0;	/* change file name */
static int g_file_count = 0;  /* number of save files */
unsigned char* g_img_buf = NULL;
static unsigned char jpegQuality = 70;

#define S5K_CTRL_NUM 4
int g_brightness = 0xDEAD; 
int g_contrast = 0;
int g_saturation = 0;
int g_sharpness = 0;
	
struct v4l2_control s5k_ctrl[8]= {
	{ V4L2_CID_BRIGHTNESS, 0},
	{ V4L2_CID_CONTRAST, 0},
	{ V4L2_CID_SATURATION, 0},
	{ V4L2_CID_SHARPNESS, 0},
};

static void errno_exit(const char *s)
{
	fprintf (stderr, "%s error %d, %s\n",
			s, errno, strerror (errno));
	exit (EXIT_FAILURE);
}

static void fill_ctrls(void)
{
	s5k_ctrl->value = g_brightness;
	(s5k_ctrl+1)->value = g_contrast;
	(s5k_ctrl+2)->value = g_saturation;
	(s5k_ctrl+3)->value = g_sharpness;
}

static void start_control_testing(void)
{
	struct v4l2_format fmt;
	int i, ret = 0;

	PR_IO(VIDIOC_G_FMT);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(g_file_desc, VIDIOC_G_FMT, &fmt);
	if (ret) {
		errno_exit("Failed to read format");
		return;
	}

	fill_ctrls();
	
	printf(" buffer parameters: %dx%d plane[0]=%d\n",
			fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
			fmt.fmt.pix_mp.plane_fmt[0].sizeimage);

	
	printf("====== Calling S_CTRL ==================================\n");
	
	for (i = 0; i < S5K_CTRL_NUM; i++) {
		ret |= ioctl(g_file_desc, VIDIOC_S_CTRL, s5k_ctrl + i);
	}
	if (ret) {
		errno_exit(" error S_CTRL");
	}

	printf("====== Calling G_CTRL ==================================\n");
	for (i = 0; i < S5K_CTRL_NUM; i++) {
		ret |= ioctl(g_file_desc, VIDIOC_G_CTRL, s5k_ctrl + i);
		printf("  CTRL ID %d VAL %d \n", (s5k_ctrl + i)->id, (s5k_ctrl + i)->value);
	}
	if (ret) {
		errno_exit(" error G_CTRL");
	}

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
		buffers[buf_index].width  = g_pix_width;
		buffers[buf_index].height = g_pix_height;

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
        fmt.fmt.pix_mp.num_planes = 1;
	fmt.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width	= g_pix_width;
	fmt.fmt.pix_mp.height	= g_pix_height;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;

	PR_IO(VIDIOC_S_FMT);
	if (-1 == xioctl (g_file_desc, VIDIOC_S_FMT, &fmt))
		errno_exit ("VIDIOC_S_FMT");

	/* Note VIDIOC_S_FMT may change width and height. */

#if 0
	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;
#endif

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
		sprintf(&file_name[0], "422X_%d.yuv",file_loop);
	}
	else { 
		sprintf(&file_name[0], "420X%d.yuv", file_loop);
	}
	fflush(stdout);
	/* file create/open, note to "wb" */
	yuv_fp = fopen(&file_name[0], "wb");
	if (!yuv_fp) {
		perror(&file_name[0]);
	}
	fwrite(g_yuv, 1, g_pix_height * g_pix_width * bpp / 8, yuv_fp);
	fclose(yuv_fp);
}


int save_jpeg = 1;
static void jpegWrite(unsigned char* img)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	int bpp = 16;
        char file_name[100];
	FILE *outfile;
	JSAMPROW row_pointer[1];

        if (bpp == 16 ) {
		sprintf(&file_name[0], "linaro%dw%db%dc%dsa%dsp%d.jpg",
		file_loop, 
		g_pix_width,
		g_brightness, 
		g_contrast,
		g_saturation,
		g_sharpness);
        }
        else {
                sprintf(&file_name[0], "420X%d.jpg", file_loop);
        }
	outfile = fopen(&file_name[0], "wb" );

	// try to open file for saving
	if (!outfile) {
		errno_exit("jpeg");
	}

	// create jpeg data
	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, outfile);

	// set image parameters
	cinfo.image_width = g_pix_width;
	cinfo.image_height = g_pix_height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	// set jpeg compression parameters to default
	jpeg_set_defaults(&cinfo);
	// and then adjust quality setting
	jpeg_set_quality(&cinfo, jpegQuality, TRUE);

	// start compress 
	jpeg_start_compress(&cinfo, TRUE);

	// feed data
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = &img[cinfo.next_scanline * cinfo.image_width *  cinfo.input_components];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	// finish compression
	jpeg_finish_compress(&cinfo);

	// destroy jpeg data
	jpeg_destroy_compress(&cinfo);

	// close output file
	fclose(outfile);
}


/**
  Convert from YUV422 format to RGB888. Formulae are described on http://en.wikipedia.org/wiki/YUV

  \param width width of image
  \param height height of image
  \param src source
  \param dst destination
*/
static void YUV422toRGB888(int width, int height, unsigned char *src, 
									unsigned char *dst)
{
  int line, column;
  unsigned char *py, *pu, *pv;
  unsigned char *tmp = dst;

  /* In this format each four bytes is two pixels. Each four bytes is two Y's, a Cb and a Cr. 
     Each Y goes to one of the pixels, and the Cb and Cr belong to both pixels. */
  py = src;
  pu = src + 1;
  pv = src + 3;

  #define CLIP(x) ( (x)>=0xFF ? 0xFF : ( (x) <= 0x00 ? 0x00 : (x) ) )

  for (line = 0; line < height; ++line) {
    for (column = 0; column < width; ++column) {
      *tmp++ = CLIP((double)*py + 1.402*((double)*pv-128.0));
      *tmp++ = CLIP((double)*py - 0.344*((double)*pu-128.0) - 0.714*((double)*pv-128.0));
      *tmp++ = CLIP((double)*py + 1.772*((double)*pu-128.0));

      // increase py every time
      py += 2;
      // increase pu,pv every second time
      if ((column & 1)==1) {
        pu += 4;
        pv += 4;
      }
    }
  }
}

static void process_image (char *p)
{
	int bpp =16;
	
	if (!file_loop) {
		file_loop++;/* Discard first frame */ 
		return;	
	}

	if(save_jpeg) {
		unsigned char* src = (unsigned char*)p;
		// convert from YUV422 to RGB888
		YUV422toRGB888(g_pix_width, g_pix_height,src,g_img_buf);
		// write jpeg
		jpegWrite(g_img_buf);
	}
	else {
		save_yuv(bpp, p);	/* 422 */
	}
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
	unsigned int count = g_file_count;

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

	if (argc != 8 ) {  
		printf("%s width height num_of_frames bright contrast saturation sharpness \n", argv[0]);
		printf("EX)  $ %s 640 480  3  0 0 0 24612 \n", argv[0]);
		goto err;
	}

	g_pix_width = atoi(argv[1]);  
	g_pix_height = atoi(argv[2]);
	g_file_count = atoi(argv[3]) + 1;

	g_brightness = atoi(argv[4]);
	g_contrast = atoi(argv[5]);
	g_saturation = atoi(argv[6]);
	g_sharpness = atoi(argv[7]);

	g_img_buf = malloc(g_pix_width*g_pix_height*3*sizeof(char));

	open_device();
	init_v4l2_device();

	ret = init_mmap(&n_buffers);
	if(ret)	 
		errno_exit ("init_mmap error !!!");

	start_control_testing();
	start_capturing(buffers);
	mainloop();
	stop_capturing();
	close_mmap();
	close_device();
	free(g_img_buf);
	exit(EXIT_SUCCESS);

err:
	return ret;
}
