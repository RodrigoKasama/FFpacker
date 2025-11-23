/*#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>*/

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

#define TEMP_FILE "/tmp/auxiliar.h264"

typedef struct
{
	int id;
	int width;
	int height;
	double framerate;
	int nb_frames;
	int vStreamidx;
} tmpinfo_t;

// gcc -o main using_static.c -lavformat -lavutil -lavcodec
// ./main 'rtsp://admin:123456@xxx.yyy.zzz:5540/H264?ch=1&subtype=1' tcp '/home/ffmpeg' 5

double get_framerate(char *IN_filename, int video_index);
void write_temp_file(void *dados, char *dir_out, int vstrm_idx, int width, int height);

int main(int argc, char **argv)
{

	if (argc != 4 && argc != 5)
	{
		fprintf(stderr, "\n Usage: %s <url> <protocols> <current_path> -<timeout-sec>\n\n", argv[0]);
		fprintf(stderr, "   Ex: %s rtsp://1.2.3.45/video tcp,udp '/home/parracho/tarefa3' 10\n\n", argv[0]);
		fprintf(stderr, "    ** Escrito por: Rodrigo Parracho **\n\n");
		exit(1);
	}

	char *url = argv[1];
	char *protocol = argv[2];
	char *dir_output = argv[3];
	// char *temp_path = "/tmp/auxiliar.h264";
	// char final_path[100];
	// char filename[30];
	char timeout[10];
	int vstream_index = -1;

	av_register_all();
	avformat_network_init();

	AVInputFormat *in_fmt = NULL;
	AVFormatContext *in_fmt_ctx = NULL;
	AVCodecContext *codec_ctx = NULL;
	AVCodec *codec = NULL;
	AVStream *vStream = NULL;
	AVPacket *pPkt = NULL;
	AVDictionary *opts = NULL;

	if (argc == 5)
	{
		sprintf(timeout, "%s000000", argv[4]);
		av_dict_set(&opts, "stimeout", timeout, 0);
	}
	in_fmt_ctx = avformat_alloc_context();
	if (!in_fmt_ctx)
	{
		fprintf(stderr, "Couldn't get input format (rtsp)");
		exit(1);
	}

	in_fmt = av_find_input_format("rtsp");
	if (!in_fmt)
	{
		fprintf(stderr, "Error guessing input format.\n");
		exit(1);
	}

	if (!protocol)
	{
		fprintf(stderr, "Couldn't find a stable protocol to communicate with.");
		exit(1);
	}

	// Acessando via protocolo..
	av_dict_set(&opts, "rtsp_transport", protocol, 0);

	fprintf(stdout, "Trying transport protocol %s.. \n", protocol);

	if (avformat_open_input(&in_fmt_ctx, url, in_fmt, &opts) != 0)
	{
		fprintf(stderr, "Failed on connection using %s.\n", protocol);
		exit(1);
	}
	fprintf(stdout, "Connection succeded.\n Protocol Used: %s.\n", protocol);

	fprintf(stdout, "Finding stream infomation...\n");
	if (avformat_find_stream_info(in_fmt_ctx, NULL) < 0)
	{
		fprintf(stderr, "Couldn't find stream information\n");
		exit(1);
	}

	// Encontra o indice da stream dentro do container
	fprintf(stdout, "Finding stream index...\n");
	int ret = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (ret < 0)
	{
		fprintf(stderr, "Couldn't find %s stream in input file.\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		exit(1);
	}

	vstream_index = ret;

	fprintf(stdout, "Creating stream instance...\n");
	vStream = in_fmt_ctx->streams[vstream_index];
	if (!vStream)
	{
		fprintf(stderr, "Couldn't find video stream of the input, aborting\n");
		exit(1);
	}

	/*
	// Adição do AVCodecParameters
		fprintf(stdout, "Getting codec parameters...\n");
		codec_param = vStream->codecpar;
		if (!codec_param) {
			fprintf(stderr, "Couldn't define Codec Parameters");
			exit(1);
		}
	*/

	fprintf(stdout, "Finding video decoder...\n");
	codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!codec)
	{
		fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		return AVERROR(EINVAL);
	}

	codec_ctx = avcodec_alloc_context3(codec);
	/* Find decoder for the stream */
	if (!codec_ctx)
	{
		// fprintf(stderr, "Couldn't create a Codec Context with Codec Parameters\n");
		fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		exit(1);
	}

	/*
	// Para versões mais novas..
	fprintf(stdout, "Filling decoder parameters...\n");
	if (avcodec_parameters_to_context(codec_ctx, codec_param) < 0) {
		fprintf(stderr, "Erro");
		exit(1);
	}
	*/

	fprintf(stdout, "Opening stream with decoder %s...\n", codec->name);
	if ((ret = avcodec_open2(codec_ctx, codec, &opts)) < 0)
	{
		fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		return -1;
	}

	pPkt = av_packet_alloc();

	while (av_read_frame(in_fmt_ctx, pPkt) >= 0)
	{
		if (pPkt->stream_index == vstream_index)
		{

			write_temp_file(pPkt, dir_output, vstream_index, vStream->codec->width, vStream->codec->height);
			av_packet_unref(pPkt);
		}
	}

	av_packet_free(&pPkt);
	avcodec_free_context(&codec_ctx);
	avformat_free_context(in_fmt_ctx);

	return 0;
}

double get_framerate(char *IN_filename, int video_index)
{

	AVFormatContext *fmt_ctx = avformat_alloc_context();
	AVStream *video_stream = NULL;
	double fps = 0;

	if (!fmt_ctx)
	{
		fprintf(stderr, "Couldn't get input format (rtsp)");
		return -1;
	}
	if (avformat_open_input(&fmt_ctx, IN_filename, NULL, NULL) != 0)
	{
		fprintf(stderr, "Couldn't open file...\n");
		return -1;
	}
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
	{
		fprintf(stderr, "Couldn't find stream information\n");
		return -1;
	}
	video_stream = fmt_ctx->streams[video_index];
	if (!video_stream)
	{
		fprintf(stderr, "Couldn't find video stream of the input, aborting\n");
		return -1;
	}

	fps = av_q2d(video_stream->avg_frame_rate);
	avformat_free_context(fmt_ctx);
	return fps;
}

void write_temp_file(void *dados, char *dir_out, int vstrm_idx, int width, int height)
{
	static int fd = 0;
	static int counter = 0;

	/* RESOLVER */
	static int counter_files = 0;

	if (fd == 0)
	{
		counter = 0;
		fd = open_temp();
	}

	if (try_write_temp(dados, fd, 3) != 0)
	{
		fprintf(stderr, "Não foi possivel escrever o pacote recebido.\n");
		return -1;
	}
	// |
	// |
	// V
	////////////////////////////////////////////////////

	// ssize_t writen_data = 0;
	// for (int tentat = 0; writen_data != ((AVPacket *)dados)->buf->size && tentat < 3; tentat++)
	// {
	//		writen_data = write(fd, ((AVPacket *)dados)->buf->data, ((AVPacket *)dados)->buf->size);
	// }

	// if(writen_data != ((AVPacket *)data)->buf->size)
	// {
	//		fprintf(stderr, "Não foi possivel escrever o pacote recebido.\n");
	//		return -1;
	// }
	////////////////////////////////////////////////////

	counter++;

	printf("FD: %d\n", fd);

	if (((AVPacket *)dados)->flags == AV_PKT_FLAG_KEY && counter != 0)
	{
		close(fd);

		static tmpinfo_t temp_arq = {0};

		temp_arq.width = width;
		temp_arq.height = height;
		temp_arq.vStreamidx = vstrm_idx;
		temp_arq.framerate = get_framerate(TEMP_FILE, temp_arq.vStreamidx);

		// fps = get_framerate(TEMP_FILE, vstrm_idx);

		mover_tmpf(&temp_arq);
		// |
		// |
		// V
		////////////////////////////////////////////////////
		// sprintf(filename, "file_%d_%.2f_%dx%d_%d.h264", counter + 1, fps, width, height, counter_files + 1);

		// strcat(final_path, filename);

		// rename(TEMP_FILE, final_path);
		//////////////////////////////////////////////////////////

		// Zera a contagem
		counter = 0;

		// Incrementa o indice do output
		counter_files++;

		// Abre o novo dir_output e escreve o mesmo pacote, já que ele é keyframe!
		fd = open_temp();

		if (fd < 0)
		{
			if (try_write_temp(dados, fd, 3) != 0)
			{
				fprintf(stderr, "Não foi possivel escrever o pacote recebido.\n");
				return -1;
			}
		}
	}
}

int mover_tmpf(tmpinfo_t *tmp_file)
{
	char filename[128] = {0};

	// tmpinfo_t info_file = get_temp_info(); // Preenche os dados do arquivo temporário.

	// Monta as propriedades do arquivo temporario
	sprintf(filename, "File_%d_%.2f_%dx%d_%010d.h264", tmp_file->nb_frames, tmp_file->framerate, tmp_file->width, tmp_file->height, tmp_file->id);

	// Move o bloco para um local com nome das propriedades () CAMINHO FINAL.
	rename(TEMP_FILE, filename);

	return 0;
}

int open_temp()
{
	int fd = open(TEMP_FILE, O_CREAT | O_WRONLY | O_TRUNC | O_SYNC, S_IRWXU | S_IRWXG | S_IRWXO);

	if (fd < 0)
	{
		/* USAR perror() */
		fprintf(stderr, "Erro em instanciar o arquivo temp\n");
		return -1;
	}
	return fd;
}

int try_write_temp(void *data, int fd, int limit_tentat)
{
	/*  Tenta N vezes escrever o pacote e envia o feedback */
	ssize_t writen_data = 0;
	int tent = 0;

	for (; writen_data != ((AVPacket *)data)->buf->size && tent < limit_tentat; tent++)
	{
		writen_data = write(fd, ((AVPacket *)data)->buf->data, ((AVPacket *)data)->buf->size);
	}
	// Pode ter erro nesse OU... remover caso erro...
	if (writen_data != ((AVPacket *)data)->buf->size || tent == limit_tentat)
	{
		return -1;
	}
	return 0;
}