#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

// gcc -o main rtsp_try.c -lavformat -lavutil -lavcodec
// ./rtsp_try 'rtsp://admin:123456@movplay.com.br:5540/H264?ch=1&subtype=1' tcp,udp 300 'saida_padrao'
// 2.8*PTS
int main(int argc, char **argv)
{
	if (argc != 7 && argc != 8)
	{
		fprintf(stderr, "\nArgumentos informados: %d\n", argc);
		fprintf(stderr, "   Usage: %s <url> <protocols> <split_frames> <output_pattern> <dst_dir_path> <filelist_path> -<timeout-sec>\n\n", argv[0]);
		fprintf(stderr, "   Ex: %s rtsp://1.2.3.45/video tcp,udp 300 SAIDA 10\n\n", argv[0]);
		fprintf(stderr, "   ** Escrito por: Rodrigo Parracho **\n\n");
		exit(1);
	}

	char *url = argv[1]; //"rtsp://admin:123456@movplay.com.br:5540/H264?ch=1&subtype=1";
	// Não fazer hardcode nos protocolos, resulta em segfault no strtok()
	char *protocols = argv[2];
	int counter_frames = atoi(argv[3]); // 300; // 349 -> Numero primo grande, para o vídeo não ser muito curto..
	char aux_filename[20];
	strcpy(aux_filename, argv[4]);
	char *dir_path = argv[5];
	char *listfile_path = argv[6];

	char output_path[40];
	char output_filename[30];

	char timeout[10];
	char *curr_protocol = NULL;
	int vstream_index = -1;

	avformat_network_init();

	AVInputFormat *in_fmt = av_find_input_format("rtsp");
	AVFormatContext *in_fmt_ctx = avformat_alloc_context();
	AVCodecContext *codec_ctx = NULL;
	AVCodecParameters *codec_param = NULL;
	AVCodec *codec = NULL;
	AVStream *vStream = NULL;
	AVPacket *pPkt = NULL, *pPkt_keyframe = NULL;
	AVFrame *pFrm = NULL;
	AVDictionary *opts = NULL;
	FILE *fd, *fd_listfile;
	// AVFormatContext *out_fmt_ctx = NULL;

	fd_listfile = fopen(listfile_path, "ab");
	if (!fd_listfile)
	{
		fprintf(stderr, "Nao foi possivel abrir/criar a lista de registro de arquivos '%s'", listfile_path);
		exit(1);
	}

	if (argc == 8)
	{
		sprintf(timeout, "%s000000", argv[7]);
		av_dict_set(&opts, "stimeout", timeout, 0);
	}

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

	// Acessando via protocolos
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

	fprintf(stdout, "Getting codec parameters...\n");
	codec_param = vStream->codecpar;
	if (!codec_param)
	{
		fprintf(stderr, "Couldn't define Codec Parameters");
		exit(1);
	}

	fprintf(stdout, "Finding video decoder...\n");
	codec = avcodec_find_decoder(codec_param->codec_id);
	if (!codec)
	{
		fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		return AVERROR(EINVAL);
	}

	/* Find decoder for the stream */
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

	pPkt = av_packet_alloc();
	int count = 0, respPack = 0, count_files = 0;

	for (int i = 0; i < argc; i++)
	{
		fprintf(stderr, "%s\n", argv[i]);
	}

	sprintf(output_filename, "%s_%d.h264", aux_filename, count_files + 1);
	sprintf(output_path, "%s/%s", dir_path, output_filename);
	sprintf(output_filename, "file '%s'", output_filename);

	fd = fopen(output_path, "wb");
	if (!fd)
	{
		fprintf(stderr, "Nao foi possivel criar o arquivo %s", output_path);
		exit(1);
	}
	else
	{
		fwrite(output_filename, 1, sizeof(output_filename), fd_listfile);
		fwrite('\n', 1, sizeof(char), fd_listfile);
	}

	while (1)
	{
		respPack = av_read_frame(in_fmt_ctx, pPkt);
		if (respPack < 0)
			break;

		if (pPkt->stream_index == vstream_index)
		{
			// Se ainda não chegou na quantidade de frames pedida, continue escrevendo..
			if (count < (counter_frames - 1))
			{
				// fprintf(stdout, "Package %d -> HAS_KEY: %d Size: %dB\n", count, pPkt->flags, pPkt->size);
				fwrite(pPkt->buf->data, pPkt->buf->size, 1, fd);
			}
			else
			{
				// Quando chegar na qntd de frames pedidos
				// Caso o pacote de fechamento tenha um keyframe, só escreva, feche o fd atual, crie e escreva no novo arquivo.
				if (pPkt->flags == AV_PKT_FLAG_KEY)
				{
					// Como o "último" pacote é um keyframe, não é preciso buscar outro
					// Escreve o ultimo pacote e fecha o fd

					// fprintf(stdout, "Frame %d -> KEY: %d Size: %dB\n", count, pPkt->flags, pPkt->size);
					fwrite(pPkt->buf->data, pPkt->buf->size, 1, fd);
					fclose(fd);

					// Zera a contagem
					count = 0;
					// Incrementa o indice do output
					count_files++;

					// Monta o novo output_path
					sprintf(output_filename, "%s_%d.h264", aux_filename, count_files + 1);
					sprintf(output_path, "%s/%s", dir_path, output_filename);
					// sprintf(output_path, "%s/%s_%d.h264", dir_path, aux_filename, count_files + 1);
					// strcat(output_path, aux_filename);
					// sprintf(output_path, "SAIDA_%d.h264", count_files + 1);

					// Abre o novo output_path e escreve o mesmo pacote, já que ele é keyframe!
					fd = fopen(output_path, "wb");
					if (!fd)
					{
						fprintf(stderr, "Nao foi possivel abrir o arquivo %s", output_path);
						exit(1);
					}
					else
					{
						fwrite(output_filename, 1, sizeof(output_filename), fd_listfile);
						fwrite('\n', 1, sizeof(char), fd_listfile);
					}
					// fprintf(stdout, "Frame %d -> KEY: %d Size: %dB\n", count, pPkt->flags, pPkt->size);
					fwrite(pPkt->buf->data, pPkt->buf->size, 1, fd);
					// Depois, volta a contar/escrever pacotes até chegar no "último"
				}
				// Caso o pacote de fechamento não seja um keyframe
				// else
				// {
				//
				// }
			}
			count++;
			av_packet_unref(pPkt);
		}
	}

	if (fd)
	{
		fclose(fd);
	}
	if (fd_listfile)
	{
		fclose(fd_listfile);
	}

	// av_frame_free(&pFrm);
	av_packet_free(&pPkt);
	avcodec_free_context(&codec_ctx);
	avformat_free_context(in_fmt_ctx);

	return 0;
}
