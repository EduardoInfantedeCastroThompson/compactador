//Eduardo Infante de Castro Thompson 2310826
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct No {
    unsigned char c;
    int freq;
    struct No *esq, *dir;
} No;

typedef struct {
    FILE *arquivo;
    unsigned char buffer;
    int pos_bit;
} BitStream;

void escreverBit(BitStream *bs, int bit) { //função para escrever um bit no arquivo
    if (bit) bs->buffer |= (1 << (7 - bs->pos_bit)); // coloca o bit no buffer
    bs->pos_bit++;
    if (bs->pos_bit == 8) {
        fwrite(&bs->buffer, 1, 1, bs->arquivo);
        bs->buffer = 0; bs->pos_bit = 0;
    }
}

int lerBit(BitStream *bs) {
    if (bs->pos_bit == 0) {
        if (fread(&bs->buffer, 1, 1, bs->arquivo) != 1) return -1; // fim do arquivo
        bs->pos_bit = 8;
    }
    return (bs->buffer >> (--bs->pos_bit)) & 1; // lê o bit mais significativo
}

No *criaNo(unsigned char c, int freq, No *e, No *d) {
    No *n = malloc(sizeof(No));
    n->c = c; n->freq = freq; n->esq = e; n->dir = d; // inicializa o nó
    return n;
}

void escreverArvore(BitStream *bs, No *n) { // função para escrever a árvore de Huffman no arquivo
    if (!n->esq && !n->dir) { // é uma folha
        escreverBit(bs, 1); // indica que é uma folha (=1)
        if (bs->pos_bit != 0) { fwrite(&bs->buffer, 1, 1, bs->arquivo); bs->buffer=0; bs->pos_bit=0; }// alinha o buffer
        fwrite(&n->c, 1, 1, bs->arquivo);
    } else {
        escreverBit(bs, 0); // indica que não é uma folha (=0)
        escreverArvore(bs, n->esq);
        escreverArvore(bs, n->dir);
    }
}

No *reconstruirArvore(BitStream *bs) {
    int bit = lerBit(bs);
    if (bit == -1) return NULL;
    if (bit == 1) {
        // Alinhar ao próximo byte como foi feito na escrita
        if (bs->pos_bit != 0) {
            bs->pos_bit = 0;
        }
        unsigned char c;
        if (fread(&c, 1, 1, bs->arquivo) != 1) {
            fprintf(stderr, "ERRO: Não foi possível ler o caractere da folha\n");
            return NULL;
        }
        return criaNo(c, 0, NULL, NULL);
    } else {
        No *e = reconstruirArvore(bs), *d = reconstruirArvore(bs);
        return criaNo(0, 0, e, d);
    }
}


void gerarCodigos(No *n, unsigned char *codigo, int pos, unsigned char tabela[256][256], int tamanho[256]) {
    if (!n->esq && !n->dir) {
        for (int i=0; i<pos; i++) {
            tabela[n->c][i/8] |= codigo[i] << (7-(i%8)); //Escreve o código na tabela para o caractere atual
            //agora imprimir cada char
        }
        tamanho[n->c] = pos;
        //imprime o código do caractere em binário
        printf("\n Codigo do caractere '%c' (%02d): ", (n->c >= 32 && n->c < 127) ? n->c : '.', n->c);
        for (int i=0; i<pos; i++) {
            printf("%d", codigo[i]);
        }
        //imprimir os bits correspondentes a cada caractere
        printf(" (tamanho: %d bits)\n", pos);
        tabela[n->c][pos/8] |= 0;
        return;
    }
    if (n->esq) { codigo[pos]=0; gerarCodigos(n->esq, codigo, pos+1, tabela, tamanho); }
    if (n->dir) { codigo[pos]=1; gerarCodigos(n->dir, codigo, pos+1, tabela, tamanho); }
}

void liberar(No *n) {
    if (!n) return;
    liberar(n->esq); liberar(n->dir); free(n);
}

No *construirArvoreHuffman(int freq[256]) {
    No *heap[256]; int n=0;
    for (int i=0;i<256;i++) if (freq[i]) heap[n++] = criaNo(i, freq[i], NULL, NULL);
    while (n>1) {
        // acha os 2 menores
        int min1=0, min2=1; if (heap[min2]->freq < heap[min1]->freq) {int t=min1;min1=min2;min2=t;}
        for(int i=2;i<n;i++) {
            if (heap[i]->freq < heap[min1]->freq) {min2=min1;min1=i;}
            else if (heap[i]->freq < heap[min2]->freq) min2=i;
        }
        No *novo=criaNo(0, heap[min1]->freq + heap[min2]->freq, heap[min1], heap[min2]);
        heap[min1]=novo; heap[min2]=heap[n-1]; n--;
    }
    return n==1?heap[0]:NULL;
}

int main() {
    const char *entrada="texto.txt", *compactado="teste.comp", *descompactado="teste_descomp.txt";

    // Lê entrada e calcula frequência
    FILE *f=fopen(entrada,"rb"); if (!f){puts("ERRO: não achei o arquivo"); return 1;}
    int freq[256]={0}; unsigned char c;
    puts("\n==== CONTEUDO ORIGINAL ====");
    while (fread(&c,1,1,f)) {
        printf("%c", (c >= 32 && c < 127) ? c : '.');
        freq[c]++;
    }
    puts("\n\n==== FREQUENCIAS ====");
    for (int i=0;i<256;i++) if (freq[i]) printf("'%c' (%d): %d\n", (i>=32&&i<127)?i:'.',i, freq[i]);
    rewind(f);

    // Cria árvore de Huffman
    No *raiz=construirArvoreHuffman(freq);
    if (!raiz) {puts("ERRO: não conseguiu criar árvore!"); fclose(f); return 1;}

    // Gera códigos
    unsigned char tabela[256][256]={{0}}; int tamanho[256]={0}; unsigned char cod[256]={0};
    gerarCodigos(raiz,cod,0,tabela,tamanho); //gera tabela de códigos

    // Compacta arquivo
    FILE *fc=fopen(compactado,"wb"); BitStream bsOut={fc,0,0};
    escreverArvore(&bsOut,raiz); // escreve a árvore no arquivo compactado 
    if (bsOut.pos_bit>0) fwrite(&bsOut.buffer,1,1,fc);
    fseek(f,0,SEEK_END); unsigned tam=ftell(f); rewind(f);
    unsigned char ts[4]={tam>>24,tam>>16,tam>>8,tam}; fwrite(ts,1,4,fc);
    while(fread(&c,1,1,f)){
        for(int i=0;i<tamanho[c];i++){
            int bit=(tabela[c][i/8]>>(7-(i%8)))&1;
            escreverBit(&bsOut,bit); // escreve cada bit do código do caractere
        }
    }
    if (bsOut.pos_bit>0) fwrite(&bsOut.buffer,1,1,fc); // escreve o último byte se necessário
    fclose(fc); fclose(f);
    //TEMOS NOSSO ARQUIVO COMPACTADO! 
    // Imprime arquivo compactado
    puts("\n==== ARQUIVO COMPACTADO (bytes em hexadecimal) ====");
    fc=fopen(compactado,"rb");
    if (!fc) {puts("ERRO: não consegui abrir arquivo compactado!"); return 1;}
    int b; int col=0;
    while ((b=fgetc(fc))!=EOF) {
        printf("%02X ", b);
        if (++col%16==0) puts("");
    }
    fclose(fc);

    // Descompacta
    FILE *fd=fopen(compactado,"rb"), *fo=fopen(descompactado,"wb");
    BitStream bsIn={fd,0,0}; No *r2=reconstruirArvore(&bsIn);
    unsigned char t2[4]; for(int i=0;i<4;i++){t2[i]=0;for(int j=0;j<8;j++){int b=lerBit(&bsIn);t2[i]=(t2[i]<<1)|b;}}//lê 4 bytes indicam tamanho arq
    unsigned total=(t2[0]<<24)|(t2[1]<<16)|(t2[2]<<8)|t2[3];// lê tamanho do arquivo descompactado
    No *atual=r2; unsigned dec=0;
    while (dec<total && (b=lerBit(&bsIn))!=-1){ 
        atual = b?atual->dir:atual->esq;
        if (!atual->esq && !atual->dir){fputc(atual->c,fo); atual=r2; dec++;}
    }
    fclose(fd); fclose(fo);

    // Imprime arquivo descompactado
    puts("\n\n==== CONTEUDO DESCOMPACTADO ====");
    fo=fopen(descompactado,"rb");
    if (!fo) {puts("ERRO: não consegui abrir arquivo descompactado!"); return 1;}
    while ((b=fgetc(fo))!=EOF) printf("%c", (b>=32&&b<127)?b:'.');
    fclose(fo);

    liberar(raiz); liberar(r2);
    puts("\n\nCompactacao e descompactacao concluidas!");
    return 0;
}
