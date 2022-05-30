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
    int stream_lenght = frame->height * frame->width; // * 3;
    unsigned char vetBytes[stream_lenght];

    fprintf(stdout, "%dx%d=%d pixels reconhecidos...\n", frame->width, frame->height, stream_lenght);

    fprintf(stdout, "Lendo Camada de Gama...\n");
    /* Y */
    for (int i = 0; i < frame->height; i++)
    {
        fprintf(stderr, "Linha %d -> ", i);
        for (int j = 0; j < frame->width; j++)
        {
            vetBytes[frame->width * i + j] = frame->data[0][frame->linesize[0] * i + j];
            fprintf(stderr, "%d ", frame->data[0][frame->linesize[0] * i + j]);
        }
        fprintf(stderr, "\n");
    }
    // fprintf(stderr, "FOi aki\n");

    fd = fopen(filename, "wb");

    for (int k = 0; k < stream_lenght; k++)
    {
        fwrite(&vetBytes[k], 1, sizeof(vetBytes), fd);
    }

    fclose(fd);

    // fprintf(stderr, "FOi aki2\n");
    /* Cb e Cr */
}


static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename)
{
    FILE *f;
    int i;
    f = fopen(filename,"w");
    // writing the minimal required header for a pgm file format
    // portable graymap format -> https://en.wikipedia.org/wiki/Netpbm_format#PGM_example
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

    // writing line by line
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}


int main(int argc, char **argv)
{
    if (argc < 3 || argc > 4)
    {
        fprintf(stderr, "\n Usage: %s <url> <protocols> -<timeout-sec>\n\n", argv[0]);
        fprintf(stderr, "   Ex: %s rtsp://1.2.3.45/video tcp,udp 3 \n\n", argv[0]);
        fprintf(stderr, "    ** Escrito por: Rodrigo Parracho **\n\n");
        exit(1);
    }

    char timeout[10];
    char *url = argv[1];
    char *protocols = argv[2];
    char *curr_protocol = NULL;
    int vstream_index = -1;
    int ret = 0;

    // av_register_all();
    avformat_network_init();

    AVInputFormat *in_fmt = av_find_input_format("rtsp");
    AVFormatContext *in_fmt_ctx = avformat_alloc_context();
    AVFormatContext *out_fmt_ctx;
    AVCodecContext *codec_ctx = NULL;
    AVCodecParameters *codec_param = NULL;
    AVCodec *codec = NULL;
    AVStream *vStream = NULL;
    AVPacket *pPkt = NULL;
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

    if (argc == 4)
    {
        sprintf(timeout, "%s000000", argv[3]);
        av_dict_set(&opts, "stimeout", timeout, 0);
    }

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
    ret = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

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
        return ret;
    }

    // av_dump_format(in_fmt_ctx, vstream_index, stdout, 0);

    // printf("%d\nFOI\n", vstream_index);
    pPkt = av_packet_alloc();
    pFrm = av_frame_alloc();

    FILE *fd;
    AVBufferRef *buff;
    // buff = av_buffer_allocz(sizeof(uint8_t) * 1920 * 1080);
    int flag = 0;
    int respPack, respFram;
    AVFrame *aux_frame = av_frame_alloc();

    while (av_read_frame(in_fmt_ctx, pPkt) >= 0)
    {
        if (pPkt->stream_index == vstream_index)
        {
            respPack = avcodec_send_packet(codec_ctx, pPkt);
            if (respPack < 0 || flag == 1)
                break;

            while (respPack >= 0)
            {
                respFram = avcodec_receive_frame(codec_ctx, pFrm);
                if (respFram >= 0)
                {
                    fprintf(stdout, "Frame %d (type=%c, size=%d B)\n",
                            codec_ctx->frame_number,
                            av_get_picture_type_char(pFrm->pict_type),
                            pFrm->pkt_size);

                    // fprintf(stderr, "Olaa1\n");

                    if (pFrm->pict_type == AV_PICTURE_TYPE_I)
                    {

                        save_gray_frame(pFrm->data[0], pFrm->linesize[0], pFrm->width, pFrm->height, "/mnt/c/Users/Acer/Desktop/asdhskajdh.");
                        // av_frame_copy(aux_frame, pFrm);
                        // generate_img_output(aux_frame, "/home/parracho/rec_rtsp/preto_e_branco.png");

                        // int offset = pFrm->width;
                        // int stream_lenght = pFrm->height * pFrm->width; // * 3;
                        // unsigned char vetBytes[stream_lenght];

                        // fprintf(stdout, "%dx%d=%d pixels reconhecidos...\n", pFrm->width, pFrm->height, stream_lenght);

                        // fprintf(stdout, "Lendo Camada de Gama...\n");
                        // // Y
                        // for (int i = 0; i < pFrm->height; i++)
                        // {
                        //     // fprintf(stderr, "Linha %d -> ", i);
                        //     for (int j = 0; j < pFrm->width; j++)
                        //     {
                        //         vetBytes[pFrm->width * i + j] = pFrm->data[0][pFrm->linesize[0] * i + j];
                        //         fprintf(stderr, "%d ", pFrm->data[0][pFrm->linesize[0] * i + j]);
                        //     }
                        //     // fprintf(stderr, "\n");
                        //     // getchar();
                        // }

                        // fprintf(stdout, "Leitura finalizada\n");

                        // fprintf(stdout, "Iniciando escrita em arquivo...\n");
                        // fd = fopen("/home/parracho/rec_rtsp/preto_e_branco.raw", "wb");

                        // for (int k = 0; k < stream_lenght; k++)
                        // {
                        //     fwrite(&(vetBytes[k]), sizeof(unsigned char), 1, fd);
                        // }

                        // fclose(fd);

                        // fprintf(stdout, "Escrita em arquivo finalizada\n");
                        flag = 1;
                    }
                }

                else if (respFram == AVERROR(EAGAIN) || respFram == AVERROR_EOF)
                    break;
                else if (respFram < 0)
                {
                    fprintf(stderr, "Error...");
                    return -1;
                }
            }
            av_packet_unref(pPkt);
        }
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
