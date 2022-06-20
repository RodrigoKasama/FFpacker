#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

// gcc -o main split_rtsp.c -lavformat -lavutil -lavcodec
// ./split_rtsp 'rtsp://admin:123456@movplay.com.br:5540/H264?ch=1&subtype=1' tcp,udp 300 10

AVPacket **buffer_packs;
AVPacket *pPkt_keyframe;

AVPacket create_virtual_pack(FILE *arq, char *filename, AVCodecContext *codecCTX, int counter, int file_index, int buf_index);

int main(int argc, char **argv)
{
	if (argc != 4 && argc != 5)
	{
		fprintf(stderr, "\n Usage: %s <url> <protocols> <split_frames> -<timeout-sec>\n\n", argv[0]);
		fprintf(stderr, "   Ex: %s rtsp://1.2.3.45/video tcp,udp 300 10\n\n", argv[0]);
		fprintf(stderr, "    ** Escrito por: Rodrigo Parracho **\n\n");
		exit(1);
	}

	char *url = "rtsp://admin:123456@movplay.com.br:5540/H264?ch=1&subtype=1";
	// Não fazer hardcode nos protocolos, resulta em segfault no strtok()
	char *protocols = argv[2];
	int counter_frames = 300; // 349 -> Numero primo grande, para o vídeo não ser muito curto..
	char timeout[10];
	char out_filename[13];

	char *curr_protocol = NULL;
	int vstream_index = -1;

	avformat_network_init();

	AVInputFormat *in_fmt = av_find_input_format("rtsp");
	AVFormatContext *in_fmt_ctx = avformat_alloc_context();
	AVCodecContext *codec_ctx = NULL;
	AVCodecParameters *codec_param = NULL;
	AVCodec *codec = NULL;
	AVStream *vStream = NULL;
	AVPacket *pPkt = NULL;
	AVFrame *pFrm_key = NULL, *pFrm_delta = NULL;
	AVDictionary *opts = NULL;
	FILE *fd;
	// AVFormatContext *out_fmt_ctx = NULL;

	if (argc == 5)
	{
		sprintf(timeout, "%s000000", argv[4]);
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

	// codec_ctx->framerate = av_guess_frame_rate(in_fmt_ctx, vStream, NULL);
	// codec_ctx->time_base = av_inv_q(codec_ctx->framerate);

	fprintf(stdout, "Opening stream with decoder %s...\n", codec->name);
	if ((ret = avcodec_open2(codec_ctx, codec, &opts)) < 0)
	{
		fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		return -1;
	}

	// AVFrame last_key;

	// Preenche o pacote com valores default
	pPkt = av_packet_alloc();
	pFrm_key = av_frame_alloc(), pFrm_delta = av_frame_alloc();
	pPkt_keyframe = av_packet_alloc();
	int count = 0, count_files = 1;
	int aux_gop_counter = 0, gop_size = -1;
	int create_buff_flag = 0;
	int respPack = 0;
	int buffer_index = 0;

	// Monta a primeira saida
	sprintf(out_filename, "SAIDA_%d.h264", count_files);
	fprintf(stderr, "Criando e preenchendo o arquivo %s..\n", out_filename);

	fd = fopen(out_filename, "wb");

	fprintf(stderr, "FRAMERATE: %lf GOP: %d\n", av_q2d(vStream->r_frame_rate), codec_ctx->gop_size);

	// uint8_t **data_pointer;

	// AVBufferRef **packets_buffer;

	// fprintf(stderr, "olaaaa\n");
	fprintf(stderr, "Analisando GOP..\n");
	while (gop_size == -1)
	{
		if (av_read_frame(in_fmt_ctx, pPkt) < 0)
			break;

		if (pPkt->stream_index == vstream_index)
		{
			if (pPkt->flags != AV_PKT_FLAG_KEY)
			{
				aux_gop_counter++;
			}
			else if (pPkt->flags == AV_PKT_FLAG_KEY && aux_gop_counter != 0)
			{
				fprintf(stderr, "Analise completa. GOP_SIZE=%d\n", ++aux_gop_counter);
				gop_size = aux_gop_counter;
				// Declara o buffer uma unica vez...
				buffer_packs = (AVPacket **)av_mallocz(sizeof(AVPacket) * gop_size);

				if (!buffer_packs)
				{
					fprintf(stderr, "Nao foi possivel alocar memória para armazenar os pacotes de montagem...");
					exit(1);
				}

				break;
			}
		}
		av_packet_unref(pPkt);
	}

	// Mapeando e empacotando
	while (av_read_frame(in_fmt_ctx, pPkt) >= 0)
	{
		// respPack = avcodec_send_packet(codec_ctx, pPkt);
		// if (respPack < 0)
		//  // {
		//  	fprintf(stderr, "olaaaa\n");
		//  	break;
		//  }

		if (pPkt->stream_index == vstream_index)
		{
			// O código abaixo só vai executar/ escrever quando o GOP for calculado

			if (pPkt->flags == AV_PKT_FLAG_KEY)
			{
				buffer_index = 0;
				av_packet_copy_props(pPkt_keyframe, pPkt);
			}
			else
			{
				av_packet_copy_props(buffer_packs[buffer_index], pPkt);
			}
			buffer_index++;

			// Se ainda não chegou na quantidade de frames pedida, continue escrevendo..
			if (count < (counter_frames - 1))
			{
				fprintf(stdout, "Package %d -> HAS_KEY: %d Size: %dB\n", count, pPkt->flags, pPkt->size);
				fwrite(pPkt->buf->data, pPkt->buf->size, 1, fd);
			}
			else
			{
				// fprintf(stderr, "olaaaa\n");
				// Quando chegar na qntd de frames pedidos
				// Caso o pacote de fechamento tenha um keyframe, só escreva, feche o fd atual, crie e escreva novamente no novo arquivo.
				if (pPkt->flags == AV_PKT_FLAG_KEY)
				{
					// Como o "último" pacote é um keyframe, não é preciso buscar outro
					// Escreve o ultimo pacote e fecha o fd

					fprintf(stdout, "Frame %d -> HAS_KEY: %d Size: %dB\n", count, pPkt->flags, pPkt->size);
					fwrite(pPkt->buf->data, pPkt->buf->size, 1, fd);
					fclose(fd);
					fprintf(stdout, "%s criado com sucesso\n\tInfo: Frames contidos neste arquivo -> %d\n", out_filename, count);

					// Zera a contagem
					count = 0;
					// Incrementa o indice do output
					count_files++;

					// Monta o novo out_filename
					sprintf(out_filename, "SAIDA_%d.h264", count_files);
					fprintf(stdout, "Criando e preenchendo o arquivo %s..\n\n", out_filename);

					// Abre o novo out_filename e escreve o mesmo pacote, já que ele é keyframe!
					fd = fopen(out_filename, "wb");

					fprintf(stdout, "Frame %d -> KEY: %d Size: %dB\n", count, pPkt->flags, pPkt->size);
					fwrite(pPkt->buf->data, pPkt->buf->size, 1, fd);
					// Depois, volta a contar/escrever pacotes até chegar no "último"
				}
				// Caso o pacote de fechamento não seja um keyframe, gere um a partir do último keyframe (e dos P-frames precedentes?)
				else
				{
					/*Contador de passos anteriores - Rever passos desde o último keyframe*/

					// create_virtual_pack(fd, out_filename, codec_ctx, count, count_files, buffer_index);
					if (avcodec_decode_video2(codec_ctx, pFrm_key, 1, pPkt_keyframe) < 0)
					{
						fprintf(stderr, "Erro em criar um pacote virtual - Nao foi possivel decodificar no ultimo keyframe guardado\n");
						exit(1);
					}

					int offset = pFrm_key->width;
					int stream_lenght_key = pFrm_key->height * pFrm_key->width * 3;

					// uint8_t *key_frame_array = (uint8_t *)av_mallocz_array(sizeof(pFrm_key->data));
					uint8_t *key_frame_array[stream_lenght_key];

					uint8_t *delta_frame_array = av_mallocz_array(stream_lenght_key, sizeof(uint8_t));

					/* Agurpando os P-Frames para depois somar/subtrair o resultado ao keyframe calculado */
					// Começa do 1 porque o 0 é KEYFRAME
					for (int i = 1; i < buffer_index; i++)
					{

						if (avcodec_decode_video2(codec_ctx, pFrm_delta, 1, buffer_packs[i]) < 0)
						{
							fprintf(stderr, "Erro em criar um pacote virtual - Nao foi possivel decodificar no %d P-Frame do buffer\n");
							exit(1);
						}

						offset = pFrm_delta->width;

						/* GAMA */
						for (int col = 0; col < pFrm_delta->height; col++)
						{
							for (int row = 0; row < pFrm_delta->width; row++)
							{
								/* Talvez tenha que acumular os bytes de um para outro */
								delta_frame_array[(col * offset) + row + 0] += pFrm_delta->data[0][(pFrm_delta->linesize[0] * col) + row];

								/*
								delta_frame_array[(col * offset) + row + 1] = pFrm_delta->data[1][(pFrm_delta->linesize[1] * col) + row];
								delta_frame_array[(col * offset) + row + 2] = pFrm_delta->data[2][(pFrm_delta->linesize[2] * col) + row];
								*/
								//+= pFrm_delta->data[0][pFrm_delta->linesize[0] * col + row];
							}
						}

						/* Cb e Cr */
						for (int col = 0; col < pFrm_delta->height / 2; col++)
						{
							for (int row = 0; row < pFrm_delta->width / 2; row++)
							{
								// Cb
								delta_frame_array[(col * offset) + row] += pFrm_delta->data[1][(pFrm_delta->linesize[1] * col) + row];
								// Cr
								delta_frame_array[(col * offset) + row] += pFrm_delta->data[2][(pFrm_delta->linesize[2] * col) + row];
							}
						}
					}

					// Fazendo um vetor "YUV" - Primeiro adiciona o GAMA, depois o Cb e o Cr
					/* GAMA */
					for (int col = 0; col < pFrm_key->height; col++)
					{
						for (int row = 0; row < pFrm_key->width; row++)
						{
							key_frame_array[(col * offset) + row + 0] = pFrm_key->data[0][(pFrm_key->linesize[0] * col) + row];

							// /* Talvez thenha que acumular os bytes de um para outro */
							// /*
							// key_frame_array[(col * offset) + row + 1] = pFrm_key->data[1][(pFrm_key->linesize[1] * col) + row];
							// key_frame_array[(col * offset) + row + 2] = pFrm_key->data[2][(pFrm_key->linesize[2] * col) + row];
							// //+= pFrm_delta->data[0][pFrm_delta->linesize[0] * col + row];
							// */
						}
					}

					/* Cb e Cr*/
					for (int col = 0; col < pFrm_key->height / 2; col++)
					{
						for (int row = 0; row < pFrm_key->width / 2; row++)
						{
							// Cb
							key_frame_array[(col * offset) + row] = pFrm_key->data[1][(pFrm_key->linesize[1] * col) + row];
							// Cr
							key_frame_array[(col * offset) + row] = pFrm_key->data[2][(pFrm_key->linesize[2] * col) + row];
						}
					}

					// uint8_t *delta_frame_array = (uint8_t *)malloc(sizeof(pFrm_delta->data));
					// uint8_t *delta_frame_array[stream_lenght_delta];

					/* Precorrer o vetor... a cada iteração, somar os bits... */

					AVFrame *virtual_frm = av_frame_alloc();
					for (int k = 0; k < stream_lenght_key; k++)
					{
						key_frame_array[k] += delta_frame_array[k];
					}

					// int respFram = avcodec_receive_frame(codec_ctx, pFrm_delta);
					// if (respFram >= 0)
					// {
					// }
					// else if (respFram == AVERROR(EAGAIN) || respFram == AVERROR_EOF)
					// 	break;
					// else if (respFram < 0)
					// {
					// 	fprintf(stderr, "Error while receiving a frame from the decoder: %s", av_err2str(respFram));
					// }

					// fwrite(buffer_references[0]->data, buffer_references[0]->size, 1, fd);
					// fclose(fd);
					// break;
					// Dar reencode nos dois

					// avcodec_receive_frame(codec_ctx, )

					// Decodar - Acessar o frame
					// Somar os bytes
					// Encodar de novo
					// }
				}
			}
			count++;
		}
		av_packet_unref(pPkt);
	}

	if (fd)
	{
		fclose(fd);
	}

	// av_frame_free(&pFrm);
	av_packet_free(&pPkt);
	avcodec_free_context(&codec_ctx);
	avformat_free_context(in_fmt_ctx);

	return 0;
}

AVPacket create_virtual_pack(FILE *arq, char *filename, AVCodecContext *codecCTX, int counter, int file_index, int buf_index)
{
	AVFrame *pFrm = av_frame_alloc();
	int respFram;
	for (int i = 0; i < buf_index; i++)
	{
		respFram = avcodec_receive_frame(codecCTX, pFrm);

		if (respFram >= 0)
		{
		}
		else if (respFram == AVERROR(EAGAIN) || respFram == AVERROR_EOF)
			break;
		else if (respFram < 0)
		{
			fprintf(stderr, "Error while receiving a frame from the decoder: %s", av_err2str(respFram));
		}
	}
}
