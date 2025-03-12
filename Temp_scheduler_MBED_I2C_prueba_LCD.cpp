#include "mbed.h"
#include "TextLCD.h"

// Configuramos los de pines I2C (SDA y SCL)
I2C i2c_bus(PB_9, PB_8);

// Creamos un objeto para la interfaz I2C con el PCF8574
// Para la direccion debemos enviar 0|0|1|1|1|A2|A1|A0, para este caso el OCF es 0x27
// Pero la lcd reconoce son 7 bits ya que la direccion del modulo i2c en principio es de solo 6 bits, por lo tanto la direccion nueva sera 4E
TextLCD_I2C lcd(&i2c_bus, 0x27 << 1 /*4E*/, TextLCD::LCD16x2);  // LCD 16x2 con I2C

// Configuramos el puerto serial usando BufferedSerial para hacer la impresiond e valores ams sencilla
BufferedSerial serial(USBTX, USBRX, 115200);  // TX, RX, baud rate

// Definiciones de direcciones y registros del RTC DS3231M
#define DIRECCION_RTC  0xD0  
#define REG_SEGUNDOS   0x00
#define REG_MINUTOS    0x01  
#define REG_HORAS      0x02
#define REG_DIA        0x04
#define REG_MES        0x05
#define REG_ANIO       0x06 

// Direcciones de los 8 sensores de temperatura (Usamos las direcciones de los sensores definidos mediante un arreglo para poder llamarlos)
const int direccionesSensores[8] = {0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F};

// Variables de tiempo y datos
char segundosRTC, minutosRTC, horasRTC, diaRTC, mesRTC, anoRTC;
uint8_t valorSegundos, valorMinutos, valorHoras, valorDia, valorMes, valorAno;
int partesEnterasTemp[8], partesDecimalesTemp[8];
float temperaturas[8];

// Prototipos de funciones
char LeerRegistroRTC(char reg);
void EscribirRegistroRTC(char reg, char valor);
void LeerDatosRTC(void);
void LeerTemperaturas(void);
void MostrarMensajeLCD(void);

// Hilos para las diferentes tareas
Thread hiloLeerRTC;
Thread hiloLeerTemperaturas;
Thread hiloActualizarLCD;

// Mutex para proteger el acceso a las variables compartidas
Mutex mutex;

// Función que se ejecutará en un hilo para leer el RTC
void tareaLeerRTC() {
    while (true) {
        // Bloquear el acceso a las variables compartidas
        mutex.lock();

        LeerDatosRTC();

        // Liberar el bloqueo
        mutex.unlock();

        // Esperar 1 segundo antes de la siguiente lectura
        ThisThread::sleep_for(1000ms);
    }
}

// Función que se ejecutará en un hilo para leer la temperatura de los 8 sensores
void tareaLeerTemperaturas() {
    while (true) {
        // Bloquear el acceso a las variables compartidas
        mutex.lock();

        LeerTemperaturas();

        // Liberar el bloqueo
        mutex.unlock();

        // Mostramos las temperaturas por el puerto serial
        char buffer[128];  // Buffer para el mensaje serial
        for (int i = 0; i < 8; i++) {
            int length = sprintf(buffer, "Sensor %d: %02i.%02i C\n", i, partesEnterasTemp[i], partesDecimalesTemp[i]);
            serial.write(buffer, length);  // Se envia el buffer a través del puerto serial
        }

        // Esperar 1 segundo antes de la siguiente lectura
        ThisThread::sleep_for(1000ms);
    }
}

// Función que se ejecutará en otro hilo para actualizar la LCD
void tareaActualizarLCD() {
    while (true) {
        // Bloqueamos el acceso a las variables compartidas
        mutex.lock();

        MostrarMensajeLCD();

        // Liberar el bloqueo
        mutex.unlock();

        // Esperar un tiempo antes de la siguiente actualización
        ThisThread::sleep_for(500ms);
    }
}

int main(void) {
    // Encender el backlight
    lcd.setBacklight(TextLCD::LightOn);  // Enciendemos la retroiluminación
    
    lcd.cls();         // Limpiar pantalla

    // Configuración inicial del RTC con valores de ejemplo (12:54:00 am del 31 de agosto de 2024)
    valorSegundos = 0x00; // 00
    valorMinutos  = 0x07; // 54
    valorHoras    = 0x14; // 12
    valorDia      = 0x16; // 31
    valorMes      = 0x09; // Agosto 
    valorAno      = 0x24; // 2024

    EscribirRegistroRTC(REG_SEGUNDOS, valorSegundos);
    EscribirRegistroRTC(REG_MINUTOS, valorMinutos);
    EscribirRegistroRTC(REG_HORAS, valorHoras);
    EscribirRegistroRTC(REG_DIA, valorDia);
    EscribirRegistroRTC(REG_MES, valorMes);
    EscribirRegistroRTC(REG_ANIO, valorAno);

    // Iniciar los hilos para leer el RTC, leer las temperaturas y actualizar la LCD
    hiloLeerRTC.start(tareaLeerRTC);
    hiloLeerTemperaturas.start(tareaLeerTemperaturas);
    hiloActualizarLCD.start(tareaActualizarLCD);

    // El hilo principal puede realizar otras tareas, si es necesario
    while (true) {
        // El hilo principal no hace nada aquí, pero podría ejecutar otras tareas si se requiere
        ThisThread::sleep_for(1000ms);
    }
}

// Función para leer un registro del RTC
char LeerRegistroRTC(char reg) {
    i2c_bus.write(DIRECCION_RTC, &reg, 1);
    char data;
    i2c_bus.read(DIRECCION_RTC, &data, 1);
    return data;
}

// Función para escribir un valor en un registro del RTC
void EscribirRegistroRTC(char reg, char valor) {
    char cmd[2];
    cmd[0] = reg;
    cmd[1] = valor;
    i2c_bus.write(DIRECCION_RTC, cmd, 2);
}

// Función para leer los datos del RTC
void LeerDatosRTC() {
    segundosRTC = LeerRegistroRTC(REG_SEGUNDOS);
    minutosRTC = LeerRegistroRTC(REG_MINUTOS);
    horasRTC = LeerRegistroRTC(REG_HORAS);
    diaRTC = LeerRegistroRTC(REG_DIA);
    mesRTC = LeerRegistroRTC(REG_MES);
    anoRTC = LeerRegistroRTC(REG_ANIO);
}

// Función para leer la temperatura de todos los sensores
void LeerTemperaturas() {
    for (int i = 0; i < 8; i++) {
        char cmd[2] = {0x00, 0x00}; 
        i2c_bus.write(direccionesSensores[i] << 1, cmd, 1);
        i2c_bus.read(direccionesSensores[i] << 1, cmd, 2);

        // Conversión de la lectura del sensor a temperatura
        float temp = (float((cmd[0] << 8) | cmd[1]) / 256.0);
        temperaturas[i] = temp * 100;
        int temperaturaTotal = (int)temperaturas[i];
        partesEnterasTemp[i] = temperaturaTotal / 100;
        partesDecimalesTemp[i] = temperaturaTotal % 100;
    }
}

// Función para mostrar el mensaje en el LCD
void MostrarMensajeLCD(void) {
    // Mostrar la fecha
    lcd.locate(0, 0);  // Posicionar en la primera línea
    lcd.printf("%02x/%02x/20%02x", diaRTC, mesRTC, anoRTC);

    // Mostrar la hora y la temperatura del primer sensor
    lcd.locate(0, 1);  // Posicionar en la segunda línea
    lcd.printf("%02x:%02x:%02x %02i.%02i C", horasRTC, minutosRTC, segundosRTC, partesEnterasTemp[0], partesDecimalesTemp[0]);
}
