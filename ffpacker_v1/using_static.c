#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#include <fcntl.h>
#include <unistd.h>

#include <string.h>

#define TEMP_FILE ".auxiliar.h264"

typedef struct
{
	int id;
	int width;
	int height;
	int nb_frames;
	int vStreamidx;
	double framerate;
	char *dir_path;
} tmpinfo_t;

// gcc -o main prtotipo_static_fd.c -lavformat -lavutil -lavcodec
// ./main 'rtsp://admin:123456@xxx.yyy.zzz:5540/H264?ch=1&subtype=1' tcp $PWD'/' 5

int move_tmpf(tmpinfo_t *tmp_file)
{

	char filename[40];
	char final_path[128];

	// Monta as propriedades do arquivo temporario
	sprintf(filename, "File_%d_%.2f_%dx%d_%05d.h264", tmp_file->nb_frames, tmp_file->framerate, tmp_file->width, tmp_file->height, ++(tmp_file->id));

	// Move o bloco para um local com nome das propriedades () CAMINHO FINAL.
	strcpy(final_path, tmp_file->dir_path);
	strcat(final_path, filename);

	// fprintf(stderr, "Caminho: %s\n", tmp_file->dir_path);
	// fprintf(stderr, "Nome criado: %s\n", filename);
	fprintf(stderr, "Caminho Final: %s\n", final_path);

	rename(TEMP_FILE, final_path);
	return 0;
}

double get_framerate(int video_index)
{

	AVFormatContext *fmt_ctx = NULL;
	AVStream *video_stream = NULL;

	fmt_ctx = avformat_alloc_context();
	double fps = 0;

	if (!fmt_ctx)
	{
		fprintf(stderr, "Couldn't get input format (rtsp)");
		return -1;
	}
	if (avformat_open_input(&fmt_ctx, TEMP_FILE, NULL, NULL) != 0)
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

int write_temp_file(void *dados, tmpinfo_t *temp_arq, int close_flag)
{

	static int fd = 0;
	/* RESOLVER -
	Em caso de interrupção, procurar um arquivo temporário incompleto e completá-lo (KEYFRAME) e começar um novo arquivo
	*/
	static int counter_files = 0;

	if (fd == 0)
	{
		fd = open(TEMP_FILE, O_CREAT | O_WRONLY | O_TRUNC | O_SYNC, S_IRWXU | S_IRWXG | S_IRWXO);
		if (fd < 0)
		{
			/* USAR perror() */
			fprintf(stderr, "Erro em instanciar o arquivo temp.\n");
			return -1;
		}
	}

	/*
	Tenta N vezes e verifica se o arquivo foi escrito, se não ele breka;
	TODO parece que o write() não retorna o valor escrito, verificar depois...
	ssize_t writen_data = 0;
	int tent = 0;

	for (; writen_data != ((AVPacket *)dados)->buf->size && tent < 3; tent++){
		fprintf(stderr,"%d - %ld\n", tent, writen_data);
		writen_data = write(fd, ((AVPacket *)dados)->buf->data, ((AVPacket *)dados)->buf->size);
	}
	// Pode ter erro nesse OU... remover caso erro...
	if (writen_data != ((AVPacket *)dados)->buf->size){
		return -1;
	}
	return 0;
	*/

	// fprintf(stderr, "Fd = %d.\n", fd);

	// Escreve no fd..
	write(fd, ((AVPacket *)dados)->buf->data, ((AVPacket *)dados)->buf->size);
	// Incrementa o contador de frames, de acordo com a escrita
	temp_arq->nb_frames++;

	fprintf(stderr, "FD=%d\n", fd);
	// Se é para fechar, pega o framerate, fecha e move.
	if (close_flag)
	{

		// Se inverter a ordem desses dois comandos, na proxima abertura ele incrementa,

		// Pegar o framerate, fechar e mover...
		temp_arq->framerate = get_framerate(temp_arq->vStreamidx);

		close(fd);

		move_tmpf(temp_arq);
		fd = 0;
		// Zera o contador de frames - Arquivo foi movido..
		temp_arq->nb_frames = 0;
	}
	return 0;
}

int main(int argc, char **argv)
{

	if (argc != 4 && argc != 5)
	{
		fprintf(stderr, "\n Usage: %s <url> <protocols> <current_path> -<timeout-sec>\n\n", argv[0]);
		fprintf(stderr, "   Ex: %s rtsp://1.2.3.45/video tcp,udp '/home/parracho/unique_fd' 10\n\n", argv[0]);
		fprintf(stderr, "    ** Escrito por: Rodrigo Parracho **\n\n");
		exit(1);
	}

	char *url = argv[1];
	char *protocol = argv[2];
	char *dir_output = argv[3];
	char timeout[10];
	int vstream_index = -1;

	av_register_all();
	avformat_network_init();
	// av_log_set_level(AV_LOG_ERROR);

	tmpinfo_t aux_file = {0};
	aux_file.dir_path = dir_output;
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
	aux_file.vStreamidx = vstream_index;

	fprintf(stdout, "Creating stream instance...\n");
	vStream = in_fmt_ctx->streams[vstream_index];
	if (!vStream)
	{
		fprintf(stderr, "Couldn't find video stream of the input, aborting\n");
		exit(1);
	}

	aux_file.width = vStream->codec->width;
	aux_file.height = vStream->codec->height;

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

	int flag = 0, close_file = 0;
	aux_file.nb_frames = 0;

	while (av_read_frame(in_fmt_ctx, pPkt) >= 0)
	{

		if (pPkt->stream_index == vstream_index)
		{

			if (!(pPkt->flags == AV_PKT_FLAG_KEY && flag != 0))
			{
				close_file = 0;
				flag = 1;
			}
			else
			{
				close_file = 1;
			}

			write_temp_file(pPkt, &aux_file, close_file);

			/* Caso escreveu um keyframe, e preciso que o arquivo seguinte comece tambem com um keyframe, por isso escreve novamente sem fechar/mover o tmp
			Obs: solucao simples porem lógica, verificar se tem um jeito mais unix de fazer
			*/
			if (close_file == 1)
			{
				write_temp_file(pPkt, &aux_file, 0);
			}
			av_packet_unref(pPkt);
		}
	}

	av_packet_free(&pPkt);
	avcodec_free_context(&codec_ctx);
	avformat_free_context(in_fmt_ctx);

	return 0;
}