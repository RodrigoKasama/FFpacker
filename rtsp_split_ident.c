#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// gcc -o main rtsp_try.c -lavformat -lavutil -lavcodec
// ./rtsp_try 'rtsp://admin:123456@movplay.com.br:5540/H264?ch=1&subtype=1' '/home/ffmpeg' tcp,udp 5

double get_framerate_of_file(char *IN_filename, int video_index);

int main(int argc, char **argv) {

	if (argc != 4 && argc != 5) {
		fprintf(stderr, "\n Usage: %s <url> <protocols> <current_path> -<timeout-sec>\n\n", argv[0]);
		fprintf(stderr, "   Ex: %s rtsp://1.2.3.45/video tcp,udp '/home/parracho/MOVIEIT/tarefa3' 10\n\n", argv[0]);
		fprintf(stderr, "    ** Escrito por: Rodrigo Parracho **\n\n");
		exit(1);
	}

	char *url = argv[1] // "rtsp://admin:123456@movplay.com.br:5540/H264?ch=1&subtype=1";
	// Não fazer hardcode nos protocolos, resulta em segfault no strtok()
	char *protocols = argv[2];
	char *dir_output = argv[3];
	char *temp_path = "/tmp/auxiliar.h264";
	char timeout[10];
	char filename[30];
	char final_path[100];
	char *curr_protocol = NULL;
	int vstream_index = -1;

	avformat_network_init();

	FILE *fd;
	AVInputFormat *in_fmt = av_find_input_format("rtsp");
	AVFormatContext *in_fmt_ctx = avformat_alloc_context();
	AVCodecContext *codec_ctx = NULL;
	AVCodecParameters *codec_param = NULL;
	AVCodec *codec = NULL;
	AVStream *vStream = NULL;
	AVPacket *pPkt = NULL;
	AVDictionary *opts = NULL;

	if (argc == 5) {
		sprintf(timeout, "%s000000", argv[4]);
		av_dict_set(&opts, "stimeout", timeout, 0);
	}

	if (!in_fmt_ctx) {
		fprintf(stderr, "Couldn't get input format (rtsp)");
		exit(1);
	}

	if (!in_fmt) {
		fprintf(stderr, "Error guessing input format.\n");
		exit(1);
	}

	// Acessando via protocolos
	while ((curr_protocol = strtok(protocols, ",")) != NULL) {
		fprintf(stdout, "Trying transport protocol %s .. \n", curr_protocol);

		av_dict_set(&opts, "rtsp_transport", curr_protocol, 0);

		if (avformat_open_input(&in_fmt_ctx, url, in_fmt, &opts) == 0) {
			fprintf(stdout, "Connection succeded.\n Protocol Used: %s.\n", curr_protocol);
			break;
		}
		else
			curr_protocol = NULL;
	}

	if (!curr_protocol) {
		fprintf(stderr, "Couldn't find a stable protocol to communicate with.");
		exit(1);
	}

	fprintf(stdout, "Finding stream infomation...\n");
	if (avformat_find_stream_info(in_fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Couldn't find stream information\n");
		exit(1);
	}

	// Encontra o indice da stream dentro do container
	fprintf(stdout, "Finding stream index...\n");
	int ret = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "Couldn't find %s stream in input file.\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		exit(1);
	}

	vstream_index = ret;

	fprintf(stdout, "Creating stream instance...\n");
	vStream = in_fmt_ctx->streams[vstream_index];
	if (!vStream) {
		fprintf(stderr, "Couldn't find video stream of the input, aborting\n");
		exit(1);
	}

	fprintf(stdout, "Getting codec parameters...\n");
	codec_param = vStream->codecpar;
	if (!codec_param) {
		fprintf(stderr, "Couldn't define Codec Parameters");
		exit(1);
	}

	fprintf(stdout, "Finding video decoder...\n");
	codec = avcodec_find_decoder(codec_param->codec_id);
	if (!codec) {
		fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		return AVERROR(EINVAL);
	}

	/* Find decoder for the stream */
	codec_ctx = avcodec_alloc_context3(codec);
	if (!codec_ctx) {
		// fprintf(stderr, "Couldn't create a Codec Context with Codec Parameters\n");
		fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		exit(1);
	}

	fprintf(stdout, "Filling decoder parameters...\n");
	if (avcodec_parameters_to_context(codec_ctx, codec_param) < 0) {
		fprintf(stderr, "erruu");
		exit(1);
	}

	fprintf(stdout, "Opening stream with decoder %s...\n", codec->name);
	if ((ret = avcodec_open2(codec_ctx, codec, &opts)) < 0) {
		fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		return -1;
	}

	pPkt = av_packet_alloc();
	int count = 0, respPack = 0, count_files = 0;

	fd = fopen(temp_path, "wb");

	// Impede que crie um arquivo com o 1º frame -> KEYFRAME
	int flag = 1;
	double framerate = 0;

	while (av_read_frame(in_fmt_ctx, pPkt) >= 0) {

		if (pPkt->stream_index == vstream_index) {
			// fprintf(stdout, "Package %d -> HAS_KEY: %d Size: %dB\n", count, pPkt->flags, pPkt->size);

			fwrite(pPkt->buf->data, pPkt->buf->size, 1, fd);

			if (pPkt->flags == AV_PKT_FLAG_KEY && flag == 0) {
				// fprintf(stdout, "Frame %d -> KEY: %d Size: %dB\n", count, pPkt->flags, pPkt->size);
				// fwrite(pPkt->buf->data, pPkt->buf->size, 1, fd);
				fclose(fd);
				// Acessa o arquivo escrito e pega a informação do fps
				framerate = get_framerate_of_file(temp_path, vstream_index);

				// Monta o filename de acordo com as propriedades e um counter
				// Framerate com apenas 2 casas decimais..
				sprintf(filename, "file_%d_%.2f_%dx%d_%d.h264", count + 1, framerate, codec_ctx->width, codec_ctx->height, count_files + 1);

				// Forma o diretório final do bloco escrito
				strcpy(final_path, dir_output);
				strcat(final_path, filename);

				// Printa a montagem do diretório..
				// fprintf(stderr, "Arquivo: %s\nCaminho:%s\nCaminho Final:%s\n\n", filename, dir_output, final_path);

				// Move o bloco para um local com nome das propriedades () CAMINHO FINAL.
				rename(temp_path, final_path);

				// Zera a contagem
				count = 0;
				// Incrementa o indice do output
				count_files++;

				// Abre o novo dir_output e escreve o mesmo pacote, já que ele é keyframe!
				fd = fopen(temp_path, "wb");

				// fprintf(stdout, "Frame %d -> KEY: %d Size: %dB\n", count, pPkt->flags, pPkt->size);
				fwrite(pPkt->buf->data, pPkt->buf->size, 1, fd);
				// Depois, volta a escrever pacotes até chegar no "último"
			}
			flag = 0;
			count++;
			av_packet_unref(pPkt);
		}
	}

	if (fd){
		fclose(fd);
	}

	// av_frame_free(&pFrm);
	av_packet_free(&pPkt);
	avcodec_free_context(&codec_ctx);
	avformat_free_context(in_fmt_ctx);

	return 0;
}

double get_framerate_of_file(char *IN_filename, int video_index) {
	
	AVFormatContext *fmt_ctx = avformat_alloc_context();
	AVStream *vStream2 = NULL;
	double fps = 0;

	if (!fmt_ctx){
		fprintf(stderr, "Couldn't get input format (rtsp)");
		return -1;
	}
	if (avformat_open_input(&fmt_ctx, IN_filename, NULL, NULL) != 0) {
		fprintf(stderr, "Couldn't open file...\n");
		return -1;
	}
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Couldn't find stream information\n");
		return -1;
	}
	vStream2 = fmt_ctx->streams[video_index];
	if (!vStream2) {
		fprintf(stderr, "Couldn't find video stream of the input, aborting\n");
		return -1;
	}

	fps = av_q2d(vStream2->avg_frame_rate);
	avformat_free_context(fmt_ctx);
	return fps;
}
