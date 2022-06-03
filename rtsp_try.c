#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

// ./rtsp_try 'rtsp://admin:123456@movplay.com.br:5540/H264?ch=1&subtype=1' tcp,udp 5

// static int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type)

void generate_img_output(AVFrame *frame, char *filename)
{
	// AVFrame *pframe = *frame;
	FILE *fd;
	int offset = frame->width;
	int stream_lenght = frame->height * frame->width * 3;
	unsigned char vetBytes[stream_lenght];

	fprintf(stdout, "%dx%d=%d pixels reconhecidos...\n", frame->width, frame->height, stream_lenght);

	fprintf(stdout, "Lendo Camada de Gama...\n");
	/* Y */
	for (int i = 0; i < frame->height; i++)
	{
		for (int j = 0; j < frame->width; j++)
		{
			vetBytes[frame->width * i + j] = frame->data[0][frame->linesize[0] * i + j];
		}
	}

	fd = fopen(filename, "wb");

	for (int k = 0; k < stream_lenght; k++)
	{
		fwrite(&vetBytes[k], 1, sizeof(vetBytes), fd);
	}

	fclose(fd);

	/* Cb and Cr */
	for (int i = 0; i < frame->height / 2; i++)
	{
		for (int j = 0; j < frame->width / 2; j++)
		{
			frame->data[1][i * frame->linesize[1] + j];
			frame->data[2][i * frame->linesize[2] + j];
		}
	}
}

int main(int argc, char **argv)
{
	if (argc != 4 && argc != 5)
	{
		fprintf(stderr, "\n Usage: %s <url> <protocols> <split_frames> -<timeout-sec>\n\n", argv[0]);
		fprintf(stderr, "   Ex: %s rtsp://1.2.3.45/video tcp,udp 500 \n\n", argv[0]);
		fprintf(stderr, "    ** Escrito por: Rodrigo Parracho **\n\n");
		exit(1);
	}

	char timeout[10];
	char *url = "rtsp://admin:123456@movplay.com.br:5540/H264?ch=1&subtype=1";

	// Não fazer hardcode nos protocolos, resulta em segfault no strtok()
	char *protocols = argv[2];

	// Ver como converte string para int mais eficiente
	int counter_frames = 300;
	char *curr_protocol = NULL;
	int vstream_index = -1;
	char out_filename[13];

	// av_register_all();
	avformat_network_init();

	AVInputFormat *in_fmt = av_find_input_format("rtsp");
	AVFormatContext *in_fmt_ctx = avformat_alloc_context();
	AVFormatContext *out_fmt_ctx = NULL;
	AVCodecContext *codec_ctx = NULL;
	AVCodecParameters *codec_param = NULL;
	AVCodec *codec = NULL;
	AVStream *vStream = NULL;
	AVPacket *pPkt = NULL, *pPkt_keyframe = NULL;
	AVFrame *pFrm = NULL;
	AVDictionary *opts = NULL;

	if (!in_fmt_ctx)
	{
		fprintf(stderr, "Couldn't get input format (rtsp)");
		exit(1);
	}

	if (!in_fmt)
	{
		fprintf(stderr, "Error guessing input format.\n");
		exit(1);
	}

	if (argc == 5)
	{
		sprintf(timeout, "%s000000", argv[4]);
		av_dict_set(&opts, "stimeout", timeout, 0);
	}

	// Verificar a melhor forma de preencher esses dois campos NULL..
	// O out_filename vai mudar constantemente, melhor não ter ?
	// avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, out_filename);
	// if (!out_fmt_ctx)
	// {
	// 	fprintf(stderr, "Could not create output context\n");
	// 	exit(1);
	// }

	while ((curr_protocol = strtok(protocols, ",")) != NULL)
	{
		fprintf(stdout, "Trying transport protocol %s .. \n", curr_protocol);

		av_dict_set(&opts, "rtsp_transport", curr_protocol, 0);

		if (avformat_open_input(&in_fmt_ctx, url, in_fmt, &opts) == 0)
		{
			fprintf(stdout, "Connection succeded.\n Protocol Used: %s.\n", curr_protocol);
			break;
		}
		else
			curr_protocol = NULL;
	}

	if (!curr_protocol)
	{
		fprintf(stderr, "Couldn't find a stable protocol to communicate with.");
		exit(1);
	}

	fprintf(stdout, "Finding stream infomation...\n");
	if (avformat_find_stream_info(in_fmt_ctx, NULL) < 0)
	{
		fprintf(stderr, "Couldn't find stream information\n");
		exit(1);
	}

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

	fprintf(stdout, "Getting codec parameters...\n");

	codec_param = vStream->codecpar;
	if (!codec_param)
	{
		fprintf(stderr, "Couldn't define Codec Parameters");
		exit(1);
	}

	//
	//
	//
	//
	// outStream = avformat_new_stream(out_fmt_ctx, NULL);
	// if (!outStream)
	// {
	// 	fprintf(stderr, "Failed allocating output stream\n");
	// 	exit(1);
	// }
	// if (avcodec_parameters_copy(outStream->codecpar, codec_param) < 0)
	// {
	// 	fprintf(stderr, "Failed to copy codec parameters\n");
	// 	exit(1);
	// }
	//
	//
	//
	//

	fprintf(stdout, "Finding video decoder...\n");
	codec = avcodec_find_decoder(codec_param->codec_id);
	if (!codec)
	{
		fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		return AVERROR(EINVAL);
	}

	/* find decoder for the stream */
	codec_ctx = avcodec_alloc_context3(codec);
	if (!codec_ctx)
	{
		// fprintf(stderr, "Couldn't create a Codec Context with Codec Parameters\n");
		fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		exit(1);
	}

	fprintf(stdout, "Filling decoder parameters...\n");
	if (avcodec_parameters_to_context(codec_ctx, codec_param) < 0)
	{
		fprintf(stderr, "erruu");
		exit(1);
	}

	fprintf(stdout, "Opening stream with decoder %s...\n", codec->name);
	if ((ret = avcodec_open2(codec_ctx, codec, &opts)) < 0)
	{
		fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		return -1;
	}


	//
	//
	//
	//
	// if (!(out_fmt_ctx->oformat->flags & AVFMT_NOFILE))
	// {
	// 	if (avio_open(&out_fmt_ctx->pb, out_filename, AVIO_FLAG_READ_WRITE) < 0)
	// 	{
	// 		fprintf(stderr, "Could not open output file '%s'", out_filename);
	// 		exit(1);
	// 	}
	// }
	// if (avformat_write_header(out_fmt_ctx, NULL) < 0)
	// {
	// 	fprintf(stderr, "Error occurred when opening output file\n");
	// 	exit(1);
	// }
	//
	//
	//
	//

	pPkt = av_packet_alloc();
	pPkt_keyframe = av_packet_alloc();
	// pFrm = av_frame_alloc();

	FILE *fd;

	int count = 0, respPack, count_files = 0;

	sprintf(out_filename, "SAIDA_%d.h264", count_files + 1);
	fd = fopen(out_filename, "wb");

	while (1)
	{
		respPack = av_read_frame(in_fmt_ctx, pPkt);
		if (respPack < 0)
			break;

		if (pPkt->stream_index == vstream_index)
		{
			pPkt->pts = (int64_t)count;

			// Se ainda não chegou na quantidade de frames pedida, continue escrevendo
			if (count < (counter_frames - 1))
			{
				fprintf(stdout, "Package %d -> KEY: %d Size: %dB\n", count, pPkt->flags, pPkt->size);
				fwrite(pPkt->buf->data, pPkt->buf->size, 1, fd);
			}
			else
			{
				// Quando chegar na qntd de frames pedidos
				// Caso o pacote de fechamento tenha um keyframe, só escreva, feche o fd atual e escreva no novo arquivo.
				if (pPkt->flags == AV_PKT_FLAG_KEY)
				{
					// Como o "último" pacote é um keyframe, não é preciso buscar outro
					fprintf(stdout, "Frame %d -> KEY: %d Size: %dB\n", count, pPkt->flags, pPkt->size);
					// Escreve o ultimo pacote e fecha o fd
					fwrite(pPkt->buf->data, pPkt->buf->size, 1, fd);
					fclose(fd);

					count_files++;

					// Monta o novo output filename
					sprintf(out_filename, "SAIDA_%d.h264", count_files + 1);
					// fprintf(stderr, "\n%s\n", out_filename);

					// Abre o novo out_filename e escreve o mesmo pacote, já que ele é keyframe!
					fd = fopen(out_filename, "wb");
					fwrite(pPkt->buf->data, pPkt->buf->size, 1, fd);
					// Depois, volta a contar/escrever pacotes até chegar no "último"
				}
				else
				{
					// Maracutaia
				}

				//
				// if (pPkt->flags == AV_PKT_FLAG_KEY)
				// {
				// 	pPkt_keyframe = pPkt;
				// }
				//
				//

				count = 0;
			}
			// count++;
			av_packet_unref(pPkt);
		}
	}
	if (fd)
	{
		fclose(fd);
	}

	av_frame_free(&pFrm);
	av_packet_free(&pPkt);
	avcodec_free_context(&codec_ctx);
	avformat_free_context(in_fmt_ctx);

	// if (open_codec_context(&vstream_index, in_fmt_ctx, AVMEDIA_TYPE_VIDEO) < 0)
	// {
	//     fprintf(stderr, "Could not find stream information\n");
	//     exit(1);
	// }
	//
	// printf("%d\n", vstream_index);
	// printf("%d\n", codec_ctx->codec_id);
	// printf("%s\n", codec->long_name);
	// = in_fmt_ctx->streams[vstream_index];
	//
	// if (avcodec_parameters_to_context(codec_ctx, codec_param) < 0)
	// {
	//     fprintf(stderr, "Couldn't form a Codec Context from the Codec Parameters...");
	//     exit(1);
	// }
	// codec = avcodec_find_decoder(codec_ctx->codec_id);
	//
	// if (avcodec_open2(codec_ctx, codec, NULL) < 0)
	// {
	//     fprintf(stderr, "EROOO");
	//     exit(1);
	// }
	//
	//     char *curr_protocol;
	//     if (avformat_open_input(&in_fmt_ctx, argv[2], NULL, NULL) < 0)
	//     {
	//         fprintf(stderr, "Could not open source file %s\n", argv[1]);
	//         exit(1);
	//     }
	//
	//     avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, argv[3]);
	//
	//     if (!out_fmt_ctx)
	//     {
	//         exit(1);
	//     }
	//
	//     int vstream_index = -1;
	//     AVCodec *p_vidCodec = NULL;
	//     for (int i = 0; i < in_fmt_ctx->nb_streams; i++)
	//     {
	//         if (in_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
	//         {
	//             p_vidCodec = avcodec_find_decoder(in_fmt_ctx->streams[i]->codecpar->codec_id);
	//             break;
	//         }
	//     }
	//
	//     AVInputFormat in_fmt = av_find_input_format("rtsp");
	//
	//     if (!in_fmt)
	//     {
	//         fprintf(stderr, "Erro ao adivinhar o input..\n");
	//         exit(1);
	//     }
	//
	//     avformat_open_input(&in_fmt_ctx, argv[1], in_fmt, opts);
	//
	//     while ((curr_protocol = strtok(argv[2], ",")) != NULL)
	//     {
	//         fprintf(stderr, "Trying transport protocol %s .. \n", curr_protocol);
	//
	//         av_dict_set(&opts, "rtsp_transport", curr_protocol, 0);
	//
	//         if (avformat_open_input(&in_fmt_ctx, argv[1], in_fmt, &opts) == 0)
	//             break;
	//         else
	//             curr_protocol = NULL;
	//     }
	//
	//     if (!curr_protocol)
	//     {
	//         fprintf(stderr, "Couldn't find a transport protocol");
	//     }
	//
	//     if (avformat_find_stream_info(in_fmt_ctx, NULL) < 0)
	//     {
	//         fprintf(stderr, "Could not find stream information\n");
	//         goto finish;
	//     }
	//
	// finish:
	//     // if (p_vidCodec)
	//     //   avcodec_close(p_vidCodec);
	//     if (in_fmt_ctx)
	//         avformat_close_input(&in_fmt_ctx);
	return 0;
}

// static int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type)
// {
//     int ret;
//     AVStream *st;
//     AVCodecContext *dec_ctx = NULL;
//     AVCodec *dec = NULL;
//     AVDictionary *opts = NULL;
//     // av_dict_set(&opts, "err_detect", "explode", 0);
//     // av_dict_set(&opts, "xerror", "1", 0);
//     // av_dict_set(&opts, "threads", "auto", 0);
//     ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
//     if (ret < 0)
//     {
//         fprintf(stderr, "Could not find %s stream in input file.\n", av_get_media_type_string(type));
//         return ret;
//     }
//     else
//     {
//         *stream_idx = ret;
//         st = fmt_ctx->streams[*stream_idx];
//         /* find decoder for the stream */
//         if (avcodec_parameters_to_context(dec_ctx, st->codecpar) < 0)
//         {
//             printf("eruuuu");
//             exit(1);
//         }
//         // dec_ctx = st->codec;
//         dec = avcodec_find_decoder(dec_ctx->codec_id);
//         if (!dec)
//         {
//             fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(type));
//             return AVERROR(EINVAL);
//         }
//         if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0)
//         {
//             fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
//             return ret;
//         }
//     }
//     return 0;
// }
