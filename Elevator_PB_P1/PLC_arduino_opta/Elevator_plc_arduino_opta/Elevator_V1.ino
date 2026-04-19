/*
  Control de Elevador - PLC Arduino Opta 
  Leds indicadores, ambos encendidos significa que esta un paro de emergencia presionado o la cadena de seguridad
  esta abierta a lo de cadena de seguridad es referente a los dispositivos de seguridad como guardas en este
  caso son las puertas del elevador localizadas en P1 y P2
  los leds indicadores parpadean dependiendo al piso que viaje, esta estatico o prendido en el piso que se encuentre
  hay dos pulsadores o botones PB y P1, se pulsa al piso que se desea viajar, en caso de que este en ese piso hace
  caso omiso a esa instruccion y solo permite viajar al otro que piso. 
*/
// Definición de Pines de entrada y salida
//Entradas
const int BTN_PB   = A0;  // I1 pulsador de request de planta baja PB
const int BTN_P1   = A1;  // I2 pulsador de request de piso 1   P1
const int LIMITE_ABAJO = A2; // I3 (Final de carrera NC)
const int LIMITE_ARRIBA = A3;// I4 en el Opta (Final de carrera NC)
const int PARO_EMERG  = A4;  // I5 en el Opta (Boton y cadena de seguridad NC) (boton paro de emergencia, puerta P1 y puerta PB)
//Salidas
const int RELAY_SUBIR = D0;  // O1 senal de salida subir al contactor del polipasto	
const int RELAY_BAJAR = D1;  // O2 senal de salida bajar al contactor del polipasto
const int PISO_PB_LED  = D2;  // O3 led indicador de piso pb o planta baja
const int PISO_P1_LED    = D3;  // 04 led indicador de piso p1 o plant alta 

// Variables de estado del elevador
bool subiendo = false; //subiendo elevador en proceso
bool bajando = false;  //bajando elevador en proceso
int ledState = LOW;  //estado del led para el parpadeo indicador
unsigned long previousMillis = 0; 

void setup() {
  // Configuración de tipo de Entradas de cada pin 
  pinMode(BTN_PB, INPUT);  //NO
  pinMode(BTN_P1, INPUT);  //NO
  pinMode(LIMITE_ABAJO, INPUT);  //NC
  pinMode(LIMITE_ARRIBA, INPUT); //NC
  pinMode(PARO_EMERG, INPUT);    //NC

  // Configuración de tipo de Salidas de cada pin
  pinMode(RELAY_SUBIR, OUTPUT); //senal de subir al polipasto
  pinMode(RELAY_BAJAR, OUTPUT); //senal de bajar al polipasto

  pinMode(PISO_PB_LED, OUTPUT); //led de piso pb  
  pinMode(PISO_P1_LED, OUTPUT); //led de piso p1

  // Inicializar todo apagado por seguridad
  digitalWrite(RELAY_SUBIR, LOW);
  digitalWrite(RELAY_BAJAR, LOW);
  digitalWrite(PISO_PB_LED, LOW);
  digitalWrite(PISO_P1_LED, LOW);
}

void loop() {
  // Lectura de entradas
  bool b_piso_pb = digitalRead(BTN_PB);  //pulsador normalmente abierto
  bool b_piso_p1 = digitalRead(BTN_P1);  //pulsador normalmente abierto
  bool en_piso_pb = !digitalRead(LIMITE_ABAJO);  // Invierte: true si el sensor es presionado
  bool en_piso_p1 = !digitalRead(LIMITE_ARRIBA); // Invierte: true si el sensor es presionado
  bool emergencia = !digitalRead(PARO_EMERG);   // Invierte: true si el botón de paro es presionado

  // ALARMA Y PARO
  if (emergencia) {
    subiendo = false;
    bajando = false;
    digitalWrite(PISO_PB_LED, HIGH); //enciende ambos estados por que esta alarmado
    digitalWrite(PISO_P1_LED, HIGH);
  } else {
    digitalWrite(PISO_PB_LED, LOW); // en caso contrario inicia leds apagados
    digitalWrite(PISO_P1_LED, LOW);
  }

  // MOVIMIENTO (Solo opera si no hay paro de emergencia o cadena de seguridad"puertas abiertas")
  if (!emergencia) {
    
    // Iniciar Subida: Se presiona botón 2, no está arriba, y NO está en estado de bajando actualmente.
    if (b_piso_p1 && !en_piso_p1 && !bajando) {
      subiendo = true;
    }
    
    // Iniciar Bajada: Se presiona botón 1, no está abajo, y NO está en estado de subiendo actualmente.
    if (b_piso_pb && !en_piso_pb && !subiendo) {
      bajando = true;
    }

    // Condiciones de Parada por medio de los sensores de limites
    if (en_piso_p1) {
      subiendo = false; // Se detiene al llegar arriba
      digitalWrite(PISO_P1_LED, HIGH);
      
    }
    if (en_piso_pb) {
      bajando = false;  // Se detiene al llegar abajo
      digitalWrite(PISO_PB_LED, HIGH);
    }
  }

  // ACTIVACION DE RELEEVADORES
  //subir
  if (subiendo && !bajando && !emergencia) {
    digitalWrite(RELAY_SUBIR, HIGH);
    digitalWrite(RELAY_BAJAR, LOW);
    blink_led(PISO_P1_LED);
  } 
  //bajar
  else if (bajando && !subiendo && !emergencia) {
    digitalWrite(RELAY_SUBIR, LOW);
    digitalWrite(RELAY_BAJAR, HIGH);
    blink_led(PISO_PB_LED);
  } 
  else {
    // Si algo falla o se detiene, apagar ambos relevadores.
    digitalWrite(RELAY_SUBIR, LOW);
    digitalWrite(RELAY_BAJAR, LOW);
    subiendo = false;
    bajando = false;
  }
  delay(50); // retardo para estabilidad de lectura´

}
//funcion para hacer que parpade el led correspondiente dependiendo al piso que este viajando
void blink_led(int ledPin) {
    unsigned long currentMillis = millis(); 
    int intervalo = 500;
    if (currentMillis - previousMillis >= intervalo) {
      previousMillis = currentMillis;
      // parpadear el led
      ledState = (ledState == LOW) ? HIGH : LOW;
      digitalWrite(ledPin, ledState);
    }
}

