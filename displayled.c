// Bibliotecas necessárias para Raspberry Pi Pico W
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h" 
#include "inc/font.h"  
#include "inc/ssd1306.h"

// Definições de hardware
#define PINO_MATRIZ 7
#define NUM_LEDS 25
#define LED_VERMELHO 13
#define LED_VERDE 11
#define LED_AZUL 12
#define BOTAO_A 5
#define BOTAO_B 6
#define PORTA_I2C i2c1
#define SDA_I2C 14
#define SCL_I2C 15
#define ENDERECO 0x3C

// Variáveis de estado
volatile bool estadoLedVerde = false;
volatile bool estadoLedAzul = false;
volatile bool debounceAtivo = false;  // Estado do debounce
const unsigned long tempoDebounce = 200; // Tempo de debounce

PIO pio = pio0;
uint sm;
uint deslocamento;

ssd1306_t display;
char ultimo_caractere = ' '; // Variável para armazenar o último caractere digitado

// Verifica caractere
bool eh_caractere_valido(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// Inicialização do display OLED
void inicializar_display() {
    ssd1306_init(&display, WIDTH, HEIGHT, false, ENDERECO, PORTA_I2C); 

    // Valida se a configuração do display foi bem-sucedida
    if (display.width == 0 || display.height == 0) {
        printf("Erro ao inicializar display OLED\n");
        return;
    }

    ssd1306_config(&display);
    ssd1306_send_data(&display);
    ssd1306_fill(&display, false);
    ssd1306_send_data(&display);
}


void atualizar_display() {
    ssd1306_fill(&display, false); // Limpa a tela

    // Exibir o caractere digitado em tamanho maior
    char texto_caractere[2] = {ultimo_caractere, '\0'}; 
    ssd1306_draw_large_char(&display, texto_caractere[0], 50, 10); // Exibe grande no topo

    // Exibir estado dos LEDs abaixo do caractere
    if (estadoLedVerde) {
        ssd1306_draw_string(&display, "O LED VERDE", 30, 40);
        ssd1306_draw_string(&display, "FOI LIGADO", 30, 20);
    } 
    else if (estadoLedAzul) {
        ssd1306_draw_string(&display, "O LED AZUL", 30, 20);
        ssd1306_draw_string(&display, "FOI LIGADO", 30, 40);
    } 
    else {
        ssd1306_draw_string(&display, "O LED NAO", 30, 20);
        ssd1306_draw_string(&display, "ESTA LIGADO", 30, 40);
    }

    ssd1306_send_data(&display); // Atualiza o display
}

// Números para a matriz de LEDs
const uint32_t formatos_numeros[10][NUM_LEDS] = {
    {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 0
    {0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0}, // 1
    {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 2 
    {1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 3
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1}, // 4
    {1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1}, // 5
    {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1}, // 6
    {0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1}, // 7
    {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 8
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}  // 9
};

// Inicializa a Matriz WS2812
void inicializar_matriz() {
    deslocamento = pio_add_program(pio, &ws2812_program);
    sm = pio_claim_unused_sm(pio, true);
    ws2812_program_init(pio, sm, deslocamento, PINO_MATRIZ, 800000, false);
}

// Exibe um número na Matriz WS2812
void exibir_numero(uint8_t num) {
    static uint8_t ultimo_numero = 255;
    if (num > 9 || num == ultimo_numero) return;

    ultimo_numero = num;
    for (int i = 0; i < NUM_LEDS; i++) {
        uint32_t cor = formatos_numeros[num][i] ? 0x0000FF : 0x000000;
        pio_sm_put_blocking(pio, sm, cor << 8);
    }
}

// Debounce por temporizador
int64_t temporizador_debounce_callback(alarm_id_t id, void *dados_usuario) {
    debounceAtivo = false;  // Libera o botão para ser pressionado novamente
    return 0;
}


// Função de interrupção para ambos os botões
void callback_gpio(uint gpio, uint32_t eventos) {
    if (debounceAtivo) return;
    debounceAtivo = true;

    add_alarm_in_ms(tempoDebounce, temporizador_debounce_callback, NULL, false);

    if (gpio == BOTAO_A) {
        if (gpio_get(BOTAO_A) == 0) { // Verifica se o botão realmente está pressionado
            estadoLedVerde = !estadoLedVerde;
            gpio_put(LED_VERDE, estadoLedVerde);
            printf("Botão A. LED Verde %s\n", estadoLedVerde ? "Ligado" : "Desligado");
        }
    } 
    else if (gpio == BOTAO_B) {
        if (gpio_get(BOTAO_B) == 0) { // Verifica se o botão realmente está pressionado
            estadoLedAzul = !estadoLedAzul;
            gpio_put(LED_AZUL, estadoLedAzul);
            printf("Botão B. LED Azul %s\n", estadoLedAzul ? "Ligado" : "Desligado");
        }
    }

    atualizar_display(); 
}


int main() {
    
    stdio_init_all();

    // Inicializa I2C
    i2c_init(PORTA_I2C, 400 * 1000);
    gpio_set_function(SDA_I2C, GPIO_FUNC_I2C);
    gpio_set_function(SCL_I2C, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_I2C);
    gpio_pull_up(SCL_I2C);

    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_put(LED_VERDE, 0);
    
    gpio_init(LED_AZUL);
    gpio_set_dir(LED_AZUL, GPIO_OUT);
    gpio_put(LED_AZUL, 0);
    
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_set_irq_enabled(BOTAO_A, GPIO_IRQ_EDGE_FALL, true);

    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);

    // Inicializa o display SSD1306
    ssd1306_init(&display, WIDTH, HEIGHT, false, ENDERECO, PORTA_I2C);
    ssd1306_config(&display);
    ssd1306_send_data(&display);
    ssd1306_fill(&display, false);
    ssd1306_send_data(&display);

    // Configuração correta da interrupção para múltiplos botões
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &callback_gpio);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &callback_gpio);

    inicializar_matriz(); // Inicializa a Matriz WS2812

    while (true) {
        char caractere_recebido = ' ';
        if (scanf("%c", &caractere_recebido) == 1 && eh_caractere_valido(caractere_recebido)) {
            ultimo_caractere = caractere_recebido;
            atualizar_display();
            if (caractere_recebido >= '0' && caractere_recebido <= '9') {
                exibir_numero(caractere_recebido - '0');
            }
        }
        sleep_ms(100);
    }
    return 0;
}
