#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define TEMP_FILE ".teste.tmp"

typedef struct
{
	int id;
	int width;
	int height;
	int frames;
} tmpinfo_t;

/************************************************************/
int FuncaoExterna__file_count_lines(char *f)
{
	// Esta função não pertence ao nosso algoritimo
	// No caso do nosso uso com ffmpeg, esta seria uma função para pegar dados do arquivo, como numero de frames..

	char ch;
	int lines = 0;
	FILE *fp = fopen(f, "r");
	if (!fp)
	{
		fprintf(stderr, "erro ao abrir arquivo pra contar linhas\n");
		return -1;
	}

	while (!feof(fp))
	{
		ch = fgetc(fp);
		if (ch == '\n')
			lines++;
	}

	fclose(fp);

	// printf("arquivo '%s' tem %d linhas\n", f, lines);

	return lines;
}
/************************************************************/

tmpinfo_t get_temp_info(void)
{
	int count_files = time(NULL) - 1659918511;
	// TODO no futuro, 'count_files' precisa sobreviver a reboots pois ele é o ID do arquivo!
	// Opção 1:  Usar time()   .... Mas o relogio pode estar errado!
	// Opção 2: Escrever o contador em um arquivo(ou fattr)  a cada "Arquivo_X_ynnnnn" novo criado

	tmpinfo_t Info = {0};

	Info.id = count_files++;
	Info.width = 320;
	Info.height = 240;
	Info.frames = FuncaoExterna__file_count_lines(TEMP_FILE);

	return Info;
}

int mover_temp(void)
{
	char filename[64] = {0}; // Para o GCC é Equivalente a memset(.., 0, ..)

	tmpinfo_t Info = get_temp_info();

	sprintf(filename, "Arquivo_%d_%d_%d_%010d.txt", Info.width, Info.height, Info.frames, Info.id);

	rename(TEMP_FILE, filename);

	return 0;
}

int write_temp_file(char *buffer, int bsize, int limit)
{

	static int fd = 0;
	static int counter = 0;

	if (fd == 0)
	{
		counter = 0;
		fd = open(TEMP_FILE, O_CREAT | O_WRONLY | O_TRUNC | O_SYNC, S_IRWXU | S_IRWXG | S_IRWXO);
		if (fd < 0)
		{
			// TODO usar perror()
			fprintf(stderr, "Erro ao criar arquivo temp\n");
			return -1;
		}
	}

	write(fd, buffer, bsize * sizeof(char)); // TODO  fazer loop gravando bytes verificando retorno de write()
	counter++;
	fprintf(stderr,"%d\n", fd);
	if (counter == limit)
	{
		close(fd);
		mover_temp();
		fd = 0;
	}

	return 0;
}

int finalizar_temp_incompleto(int truncate_linhas)
{

	int linhas = FuncaoExterna__file_count_lines(TEMP_FILE);
	if (linhas == -1)
	{
		printf("NAO EXISTE ARQUIVO TEMP A SER CONCLUIDO...\n");
		return 0;
	}

	int diferenca = truncate_linhas - linhas;
	printf("Achei uma arquivo temp incompleto com %d linhas(frames)  , deveria ter %d, a diferença é %d\n", linhas, truncate_linhas, diferenca);

	printf("Preenchendo linhas(frames)  restantes vazias ..\n");
	int fd = open(TEMP_FILE, O_WRONLY | O_APPEND);
	if (fd < 0)
	{
		fprintf(stderr, "Erro ao criar arquivo temp\n");
		return -1;
	}

	int z = 0;
	for (z = 0; z < diferenca; z++)
	{
		write(fd, "VAZIO\n", 6 * sizeof(char));
	}

	close(fd);

	mover_temp();

	return 0;
}

int main(void)
{

	// Antes de iniciar tem que tratar TEMP_FILE que ficou incompleto..
	// truncar ele com o tamanho final e mover para o nome padrão dos arquivos na ordem da contagem
	finalizar_temp_incompleto(10);

	printf(" -> INICIA CAPTURA\n");

	while (1)
	{
		write_temp_file("lalala\n", 7, 10);
		usleep(100000);
	}

	return 0;
}
