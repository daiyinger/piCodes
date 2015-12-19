/****************************************************************************
 * Copyright(c), 2001-2060, ******************************* 版权所有
 ****************************************************************************
 * 文件名称             : main.c
 * 版本                 : 0.0.1
 * 作者                 : 许龙杰
 * 创建日期             : 2014年12月24日
 * 描述                 : 
 ****************************************************************************/
#if 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <jpeglib.h>

#pragma pack(2)        //两字节对齐，否则bmp_fileheader会占16Byte
struct bmp_fileheader
{
	uint16_t bfType;        //若不对齐，这个会占4Byte
	uint32_t bfSize;
	uint16_t bfReverved1;
	uint16_t bfReverved2;
	uint32_t bfOffBits;
};

struct bmp_infoheader
{
	uint32_t biSize;
	uint32_t biWidth;
	uint32_t biHeight;
	uint16_t biPlanes;
	uint16_t biBitCount;
	uint32_t biCompression;
	uint32_t biSizeImage;
	uint32_t biXPelsPerMeter;
	uint32_t biYPelsPerMeter;
	uint32_t biClrUsed;
	uint32_t biClrImportant;
};

FILE *input_file;
FILE *output_file;

void write_bmp_header(j_decompress_ptr cinfo)
{
	struct bmp_fileheader bfh;
	struct bmp_infoheader bih;

	uint32_t width;
	uint32_t height;
	uint16_t depth;
	uint32_t headersize;
	uint32_t filesize;
	uint32_t bmpsize;

	width = cinfo->output_width;
	height = cinfo->output_height;
	depth = cinfo->output_components;

	printf("width %d, height %d, depth %d\n", width, height, depth);

	if (depth == 1)
	{
		headersize = 14 + 40 + 256 * 4;
	}

	if (depth == 3)
	{

		headersize = 14 + 40;
	}
	bmpsize = (((width * depth + 3) / 4) * 4) * height;
	filesize = headersize +  bmpsize;

	memset(&bfh, 0, sizeof(struct bmp_fileheader));
	memset(&bih, 0, sizeof(struct bmp_infoheader));

	//写入比较关键的几个bmp头参数
	bfh.bfType = 0x4D42;
	bfh.bfSize = filesize;
	bfh.bfOffBits = headersize;

	bih.biSize = 40;
	bih.biWidth = width;
	bih.biHeight = height;
	bih.biPlanes = 1;
	bih.biBitCount = (uint16_t) depth * 8;
	bih.biSizeImage = bmpsize;
	bih.biXPelsPerMeter = 0x99c1;
	bih.biYPelsPerMeter = 0x99c1;

	fwrite(&bfh, sizeof(struct bmp_fileheader), 1, output_file);
	fwrite(&bih, sizeof(struct bmp_infoheader), 1, output_file);

	if (depth == 1)        //灰度图像要添加调色板
	{
		unsigned char *platte;
		platte = (unsigned char *) malloc(256 * 4);
		unsigned char j = 0;
		int i;
		for (i = 0; i < 1024; i += 4)
		{
			platte[i] = j;
			platte[i + 1] = j;
			platte[i + 2] = j;
			platte[i + 3] = 0;
			j++;
		}
		fwrite(platte, sizeof(unsigned char) * 1024, 1, output_file);
		free(platte);
	}
}

void write_bmp_data(j_decompress_ptr cinfo, unsigned char *src_buff)
{
	unsigned char *dst_width_buff;
	unsigned char *point;

	uint32_t width;
	uint32_t height;
	uint16_t depth;

	uint32_t bufwidth;
	uint32_t bmpwidth;

	width = cinfo->output_width;
	height = cinfo->output_height;
	depth = cinfo->output_components;

	bmpwidth = width * depth;
	bufwidth = ((bmpwidth + 3) / 4) * 4;

	dst_width_buff = (unsigned char *) malloc(bufwidth);
	memset(dst_width_buff, 0, bufwidth);

	point = src_buff + bmpwidth * (height - 1);    //倒着写数据，bmp格式是倒的，jpg是正的
	uint32_t i;
	for (i = 0; i < height; i++)
	{
		uint32_t j;
		for (j = 0; j < width * depth; j += depth)
		{
			if (depth == 1)        //处理灰度图
			{
				dst_width_buff[j] = point[j];
			}

			if (depth == 3)        //处理彩色图
			{
				dst_width_buff[j + 2] = point[j + 0];
				dst_width_buff[j + 1] = point[j + 1];
				dst_width_buff[j + 0] = point[j + 2];
			}
		}
		point -= bmpwidth;
		fwrite(dst_width_buff, bufwidth, 1, output_file);    //一次写一行
	}

	free(dst_width_buff);
}

void analyse_jpeg()
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	unsigned char *src_buff;
	unsigned char *point;

	cinfo.err = jpeg_std_error(&jerr);    //一下为libjpeg函数，具体参看相关文档
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, input_file);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	uint32_t width = cinfo.output_width;
	uint32_t height = cinfo.output_height;
	uint16_t depth = cinfo.output_components;

	src_buff = (unsigned char *) malloc(width * height * depth);
	memset(src_buff, 0, sizeof(unsigned char) * width * height * depth);

/*
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE,
			width * depth, 1);
*/

	point = src_buff;
	while (cinfo.output_scanline < height)
	{
		//jpeg_read_scanlines(&cinfo, buffer, 1);    //读取一行jpg图像数据到buffer
		//memcpy(point, *buffer, width * depth);    //将buffer中的数据逐行给src_buff
		jpeg_read_scanlines(&cinfo, &point, 1);
		point += width * depth;            //一次改变一行
	}

	write_bmp_header(&cinfo);            //写bmp文件头
	write_bmp_data(&cinfo, src_buff);    //写bmp像素数据

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	free(src_buff);
}

int main(int argc, const char *argv[])
{
	if (argc < 3)
	{
		printf("%s jpg bmp\n", argv[0]);
		return -1;
	}
	input_file = fopen(argv[1], "rb");
	if (input_file == NULL)
	{
		return -1;
	}
	output_file = fopen(argv[2], "wb");
	if (output_file == NULL)
	{
		fclose(input_file);
		return -1;
	}

	analyse_jpeg();

	fclose(input_file);
	fclose(output_file);
	return 0;
}

#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int image_width, image_height;
char *image_buffer;

int read_JPEG_file (char * filename);
void write_JPEG_file (char * filename, int quality);
void put_scanline_someplace(void *buf, int count)
{
FILE *pFile = fopen("test.bin", "ab+");
fwrite(buf, count, 1, pFile);
printf("count = %d\n", count);
fclose(pFile);
return;
}

int main(int argc, char **argv)
{
int i;

printf("JPEG\n");

if ((argc > 2) && (strcmp(argv[1], "read") == 0))
{
	read_JPEG_file(argv[2]);
}

if ((argc > 1) && (strcmp(argv[1], "write") == 0))
{
	image_width = 640;
	image_height = 480;
	image_buffer = (char*) malloc(image_width * image_height * 3);

	for (i = 0; i < image_height * image_height; i++)
	{
		image_buffer[i * 3] = i * 255;
		image_buffer[i * 3 + 1] = 128 - ((i * 255) & 0x7f);
		image_buffer[i * 3 + 2] = 255 - ((i * 255) & 0xff);
	}

	printf("image_buffer [%p]\n", image_buffer);

	write_JPEG_file("w.jpg", 2);

	free(image_buffer);
}

return 0;
}
#endif
