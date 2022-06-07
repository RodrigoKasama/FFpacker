void generate_img_output(AVFrame *frame, char *filename)
{
    /**/
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
    // for (int i = 0; i < frame->height / 2; i++)
    // {
    // 	for (int j = 0; j < frame->width / 2; j++)
    // 	{
    // 		// frame->data[1][i * frame->linesize[1] + j];
    // 		// frame->data[2][i * frame->linesize[2] + j];
    // 	}
    // }
}

// Verificar a melhor forma de preencher esses dois campos NULL..
// O out_filename vai mudar constantemente, melhor não ter ?
// avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, out_filename);
// if (!out_fmt_ctx)
// {
// 	fprintf(stderr, "Could not create output context\n");
// 	exit(1);
// }

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
