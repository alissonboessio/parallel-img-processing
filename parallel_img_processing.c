#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

//#pragma pack[1]

/*------------------------------------------------------------------*/
typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
}PIXEL;

/*------------------------------------------------------------------*/

PIXEL *le_imagem_bmp(const char *arquivo, int *largura, int *altura, int *max_valor) {
    FILE *f = fopen(arquivo, "rb");
    if (f == NULL) {
        printf("Erro ao abrir o arquivo BMP: %s\n", arquivo);
        return NULL;
    }

    char header[54];
    fread(header, sizeof(char), 54, f);

    *largura = *(int*)&header[18];
    *altura = *(int*)&header[22];
    *max_valor = 255;

    int padding = (4 - (*largura * 3) % 4) % 4;
    PIXEL *img = (PIXEL *)malloc((*largura) * (*altura) * sizeof(PIXEL));

    for (int i = *altura - 1; i >= 0; i--) {
        for (int j = 0; j < *largura; j++) {
            char bgr[3];
            fread(bgr, sizeof(char), 3, f);
            img[i * (*largura) + j].r = bgr[2];
            img[i * (*largura) + j].g = bgr[1];
            img[i * (*largura) + j].b = bgr[0];
        }
        fseek(f, padding, SEEK_CUR);
    }

    fclose(f);
    return img;
}


/*------------------------------------------------------------------*/
void ordena_filtro(int *media, int cont){
	int i, j, aux;

	for(i=0; i<cont-1; i++){
		for(j=0; j<cont-1-i; j++){
			if ( media[j]>media[j+1]){
				aux = media[j];
				media[j] = media[j+1];
				media[j+1] = aux;			
			}
		}
	}
}
/*------------------------------------------------------------------*/
void * filtro_mediana(int linha, PIXEL* out, int altura, int largura, int mask, int np, PIXEL *img){
	
	int i, j, k, l;	
	
	int *mediaR=NULL, *mediaG=NULL, *mediaB=NULL;

	mediaR = (int *)malloc(mask*mask*sizeof(int));
	mediaG = (int *)malloc(mask*mask*sizeof(int));
	mediaB = (int *)malloc(mask*mask*sizeof(int));
	
	for (i=linha; i<altura; i+=np){
		for (j=0; j<largura; j++){
			
			int cont =0;

			for(k=-mask/2; k<=mask/2; k++){
				for(l=-mask/2; l<=mask/2; l++){
					if (i+k>=0 && i+k<altura && j+l>=0 && j+l<largura){
						mediaR[cont] = img[(i+k)*largura+(j+l)].r;
						mediaG[cont] = img[(i+k)*largura+(j+l)].g;
						mediaB[cont] = img[(i+k)*largura+(j+l)].b;
						cont++;
					}
				}
			}
			ordena_filtro(mediaR, cont);
			ordena_filtro(mediaG, cont);
			ordena_filtro(mediaB, cont);

			out[i*largura+j].r = mediaR[cont/2];
			out[i*largura+j].g = mediaG[cont/2];
			out[i*largura+j].b = mediaB[cont/2];
			
		}
	}

	free(mediaR);
	free(mediaG);
	free(mediaB);
}
/*------------------------------------------------------*/

void * escala_cinza(int linha, PIXEL* out, int altura, int largura, int np, PIXEL *img){	
	int i, j, k, l;
	
	for (i=linha; i<altura; i+=np){
		for (j=0; j<largura; j++){
			
			char gray = img[i*largura+j].r * 0.299 + img[i*largura+j].g * 0.587 + img[i*largura+j].b * 0.114;
			
			out[i*largura+j].r = gray;
			out[i*largura+j].g = gray;
			out[i*largura+j].b = gray;			
		}
	}
}

/*------------------------------------------------------------------*/

void escreve_imagem_bmp(const char *arquivo, PIXEL *img, int largura, int altura) {
    FILE *f = fopen(arquivo, "wb");
    if (!f) {
        printf("Erro ao criar arquivo BMP: %s\n", arquivo);
        return;
    }

    int padding = (4 - (largura * 3) % 4) % 4;
    int size = 54 + (3 * largura + padding) * altura;

    char header[54] = {
        'B', 'M',           // Assinatura
        0, 0, 0, 0,         // Tamanho do arquivo
        0, 0, 0, 0,         // Reservado
        54, 0, 0, 0,        // Offset para dados de imagem
        40, 0, 0, 0,        // Tamanho do cabeçalho DIB
        0, 0, 0, 0,         // Largura
        0, 0, 0, 0,         // Altura
        1, 0,               // Planos
        24, 0,              // Bits por pixel (24 = RGB)
        0, 0, 0, 0,         // Compressão (0 = sem)
        0, 0, 0, 0,         // Tamanho da imagem
        0, 0, 0, 0,         // Resolução X
        0, 0, 0, 0,         // Resolução Y
        0, 0, 0, 0,         // Cores na paleta
        0, 0, 0, 0          // Cores importantes
    };

    // Preencher campos dinâmicos
    *(int*)&header[2] = size;
    *(int*)&header[18] = largura;
    *(int*)&header[22] = altura;

    fwrite(header, sizeof(char), 54, f);

    char padding_data[3] = {0, 0, 0};

    for (int i = altura - 1; i >= 0; i--) {
        for (int j = 0; j < largura; j++) {
            PIXEL px = img[i * largura + j];
            char bgr[3] = { px.b, px.g, px.r };
            fwrite(bgr, sizeof(char), 3, f);
        }
        fwrite(padding_data, sizeof(char), padding, f);
    }

    fclose(f);
}

/*------------------------------------------------------------------*/
int main(int argc, char **argv){

	char entrada[50], saida[50];
	int largura, altura, max_valor, mask, i, np, shmid_out, shmid_gray, pid, id_seq, npr;
	key_t chave = 17;

	if ( argc != 5 ){
		printf("%s <img_entrada> <img_saida> <mascara> <n processos>\n", argv[0]);
		exit(0);
	}	

	strcpy(entrada, argv[1]);
	strcpy(saida, argv[2]);
	mask = atoi(argv[3]);
	npr = atoi(argv[4]);	
	
	PIXEL *img = le_imagem_bmp(entrada, &largura, &altura, &max_valor);

	shmid_gray = shmget(chave, largura *altura * sizeof(PIXEL), 0600 | IPC_CREAT);
	PIXEL *gray_out = (PIXEL *)shmat(shmid_gray, 0, 0);
	
	shmid_out = shmget(chave + 1, largura *altura * sizeof(PIXEL), 0600 | IPC_CREAT);
	PIXEL *out = (PIXEL *)shmat(shmid_out, 0, 0);
		
	id_seq = 0;
	for(i=1; i<npr; i++){
		pid = fork();
		if (pid == 0) { // filhos
			id_seq = i;
			break;
		}
	}
	
	escala_cinza(id_seq, gray_out, altura, largura, npr, img);
	filtro_mediana(id_seq, out, altura, largura, mask, npr, gray_out);
	
	if(id_seq != 0){
		shmdt(out);
		shmdt(gray_out);
	} else {
		for(i=1; i<npr; i++){
			wait(NULL);
		}
		
		escreve_imagem_bmp(saida, out, largura, altura);
		
		shmdt(gray_out);
		shmctl(shmid_gray, IPC_RMID, 0);
		shmdt(out);
		shmctl(shmid_out, IPC_RMID, 0);
	} 
	
	free(img);
}
/*--------------------------------------------------------------*/

