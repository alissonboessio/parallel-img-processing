#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

/*------------------------------------------------------------------*/

static int mask_laplacian3[9]= {
    0,-1,0,
    -1,4,-1,
    0,-1,0
};

static int mask_laplacian5[25] = {
    0,0,-1,0,0,
    0,-1,-2,-1,0,
    -1,-2,16,-2,-1,
    0,-1,-2,-1,0,
    0,0,-1,0,0
};

static int mask_laplacian7[49] = {
    0,0,0,-1,0,0,0,
    0,0,-1,-2,-1,0,0,
    0,-1,-2,-3,-2,-1,0,
    -1,-2,-3,40,-3,-2,-1,
    0,-1,-2,-3,-2,-1,0,
    0,0,-1,-2,-1,0,0,
    0,0,0,-1,0,0,0
};

/*------------------------------------------------------------------*/
#pragma pack(push, 1)
typedef struct fileheader {
	unsigned short type;
	unsigned int size_file;
	unsigned short reservad1;
	unsigned short reservad2;
	unsigned int offset;
} FILEHEADER;

typedef struct imageheader{
	unsigned int size_image_header;
	int width;
	int height;
	unsigned short planes;
	unsigned short bits_per_pixel;
	unsigned int compression;
	unsigned int image_size;
	int wresolution;
	int hresolution;
	unsigned int number_colors;
	unsigned int significant_colors;
} IMAGEHEADER;

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
}PIXEL;
#pragma pack(pop)
/*------------------------------------------------------------------*/

PIXEL *le_imagem_bmp(const char *arquivo, int *largura, int *altura) {
    FILE *f = fopen(arquivo, "rb");
    if (!f) {
        printf("Erro ao abrir o arquivo BMP: %s\n", arquivo);
        return NULL;
    }

    FILEHEADER fileHeader;
    IMAGEHEADER imageHeader;

    fread(&fileHeader, sizeof(FILEHEADER), 1, f);
    fread(&imageHeader, sizeof(IMAGEHEADER), 1, f);

    if (fileHeader.type != 0x4D42) {
        printf("Arquivo n�o � BMP v�lido!\n");
        fclose(f);
        return NULL;
    }

    *largura = imageHeader.width;
    *altura = imageHeader.height;

    int padding = (4 - (*largura * 3) % 4) % 4;
    PIXEL *img = (PIXEL *)malloc((*largura) * (*altura) * sizeof(PIXEL));

    // ir direto para os dados da img
    fseek(f, fileHeader.offset, SEEK_SET);

    for (int i = *altura - 1; i >= 0; i--) {
        for (int j = 0; j < *largura; j++) {
            char rgb[3];
            fread(rgb, sizeof(char), 3, f);
            img[i * (*largura) + j].r = rgb[2];
            img[i * (*largura) + j].g = rgb[1];
            img[i * (*largura) + j].b = rgb[0];
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

int* escolherMatrizLaplaciana(int mask){
	switch (mask) {
        case 3:
            return mask_laplacian3;
        case 5:
            return mask_laplacian5;
        case 7:
            return mask_laplacian7;
        default:
            return NULL;
    }
}

/*------------------------------------------------------------------*/

void *filtro_laplaciano(int linha, PIXEL* out, int altura, int largura, int mask, int np, PIXEL *img){
	
	int i, j, k, l;	
	
	int laplaceR, laplaceG, laplaceB;
	
	int *laplacian_mask = NULL;
	laplacian_mask =  escolherMatrizLaplaciana(mask);
		
	for (i=linha; i<altura; i+=np){
		for (j=0; j<largura; j++){			
			laplaceR = 0, laplaceG = 0, laplaceB = 0;			

			for(k=-mask/2; k<=mask/2; k++){
				for(l=-mask/2; l<=mask/2; l++){
					if (i+k>=0 && i+k<altura && j+l>=0 && j+l<largura){
						
						int laplace_value = laplacian_mask[(k+mask/2) * mask + (l+mask/2)];
						
						laplaceR += img[(i+k)*largura+(j+l)].r * laplace_value;
						laplaceG += img[(i+k)*largura+(j+l)].g * laplace_value;
						laplaceB += img[(i+k)*largura+(j+l)].b * laplace_value;
					}
				}
			}
			
			laplaceR = laplaceR < 0 ? 0 : (laplaceR > 255 ? 255 : laplaceR);
            laplaceG = laplaceG < 0 ? 0 : (laplaceG > 255 ? 255 : laplaceG);
            laplaceB = laplaceB < 0 ? 0 : (laplaceB > 255 ? 255 : laplaceB);

			out[i*largura+j].r = laplaceR;
			out[i*largura+j].g = laplaceG;
			out[i*largura+j].b = laplaceB;
			
		}
	}
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
    int image_size = (3 * largura + padding) * altura;
    int file_size = sizeof(FILEHEADER) + sizeof(IMAGEHEADER) + image_size;

    FILEHEADER fileHeader;
    fileHeader.type = 0x4D42; // 'BM'
    fileHeader.size_file = file_size;
    fileHeader.reservad1 = 0;
    fileHeader.reservad2 = 0;
    fileHeader.offset = sizeof(FILEHEADER) + sizeof(IMAGEHEADER); // normalmente 54 bytes

    IMAGEHEADER imageHeader;
    imageHeader.size_image_header = sizeof(IMAGEHEADER); // normalmente 40 bytes
    imageHeader.width = largura;
    imageHeader.height = altura;
    imageHeader.planes = 1;
    imageHeader.bits_per_pixel = 24;
    imageHeader.compression = 0;
    imageHeader.image_size = image_size;
    imageHeader.wresolution = 0;
    imageHeader.hresolution = 0;
    imageHeader.number_colors = 0;
    imageHeader.significant_colors = 0;

    // cabecalhods
    fwrite(&fileHeader, sizeof(FILEHEADER), 1, f);
    fwrite(&imageHeader, sizeof(IMAGEHEADER), 1, f);

    char padding_data[3] = {0, 0, 0};

    // dados da img
    for (int i = altura - 1; i >= 0; i--) {
        for (int j = 0; j < largura; j++) {
            PIXEL px = img[i * largura + j];
            char rgb[3] = { px.b, px.g, px.r };
            fwrite(rgb, sizeof(char), 3, f);
        }
        fwrite(padding_data, sizeof(char), padding, f);
    }

    fclose(f);
}


/*------------------------------------------------------------------*/

void esperar_todos(int *flags, int npr) {
    int done = 0;
    while (!done) {
        done = 1;
        for (int i = 0; i < npr; i++) {
            if (flags[i] == 0) {
                done = 0;
                break;
            }
        }
        usleep(1000); // Espera 1ms para evitar busy-waiting agressivo
    }
}

/*------------------------------------------------------------------*/

int main(int argc, char **argv){

	char entrada[50], saida[50];
	int largura, altura, mask, i, np, shmid_out, shmid_intermediario, pid, id_seq, npr;
	key_t chave = 17;

	if ( argc != 5 ){
		printf("%s <img_entrada> <img_saida> <mascara> <n processos>\n", argv[0]);
		exit(0);
	}	

	strcpy(entrada, argv[1]);
	strcpy(saida, argv[2]);
	mask = atoi(argv[3]);
	npr = atoi(argv[4]);	
	
	if ( mask != 3 && mask != 5 && mask != 7 ){
		printf("mask deve ser 3, 5 ou 7\n");
		exit(0);
	}		
	
	PIXEL *img = le_imagem_bmp(entrada, &largura, &altura);
	
	shmid_intermediario = shmget(chave++, largura *altura * sizeof(PIXEL), 0600 | IPC_CREAT);
	PIXEL *out_intermediario = (PIXEL *)shmat(shmid_intermediario, 0, 0);
	
	shmid_out = shmget(chave++, largura *altura * sizeof(PIXEL), 0600 | IPC_CREAT);
	PIXEL *out = (PIXEL *)shmat(shmid_out, 0, 0);
	
	int shmid_sync_flags = shmget(chave++, sizeof(int) * npr, 0600 | IPC_CREAT);
	int *sync_flags = (int *)shmat(shmid_sync_flags, NULL, 0);
	memset(sync_flags, 0, sizeof(int) * npr);

	int shmid_ready_flag_cinza = shmget(chave++, sizeof(int), 0600 | IPC_CREAT);
	int *ready_flag_cinza = (int *)shmat(shmid_ready_flag_cinza, NULL, 0);
	*ready_flag_cinza = 0;
	
	int shmid_ready_flag_mediana = shmget(chave++, sizeof(int), 0600 | IPC_CREAT);
	int *ready_flag_mediana = (int *)shmat(shmid_ready_flag_mediana, NULL, 0);
	*ready_flag_mediana = 0;
	
	id_seq = 0;
	for(i=1; i<npr; i++){
		pid = fork();
		if (pid == 0) { // filhos
			id_seq = i;
			break;
		}
	}
	
	escala_cinza(id_seq, out, altura, largura, npr, img);
	sync_flags[id_seq] = 1;
	
	if (id_seq != 0) {
		while (*ready_flag_cinza == 0) {
			usleep(1000);
		}
	} else {
		esperar_todos(sync_flags, npr);
		// Reset para proxima sync
		memset(sync_flags, 0, sizeof(int) * npr); 
		
		*ready_flag_cinza = 1;
	}	
		
	filtro_mediana(id_seq, out_intermediario, altura, largura, mask, npr, out);
	sync_flags[id_seq] = 1;
	
	if (id_seq != 0) {
		while (*ready_flag_mediana == 0) {
			usleep(1000);
		}
	} else {
		esperar_todos(sync_flags, npr);
		// Reset para proxima sync
		memset(sync_flags, 0, sizeof(int) * npr); 
		
		*ready_flag_mediana = 1;
	}	 
	
	filtro_laplaciano(id_seq, out, altura, largura, mask, npr, out_intermediario);
	
	if(id_seq == 0){
		for(i=1; i<npr; i++){
			wait(NULL);
		}
		
		escreve_imagem_bmp(saida, out, largura, altura);
		shmdt(out_intermediario);
		shmctl(shmid_intermediario, IPC_RMID, 0);
		shmdt(out);
		shmctl(shmid_out, IPC_RMID, 0);
		shmdt(sync_flags);
		shmctl(shmid_sync_flags, IPC_RMID, 0);
		shmdt(ready_flag_cinza);
		shmctl(shmid_ready_flag_cinza, IPC_RMID, 0);
		shmdt(ready_flag_mediana);
		shmctl(shmid_ready_flag_mediana, IPC_RMID, 0);

		free(img);
	} 
	
}
/*--------------------------------------------------------------*/

