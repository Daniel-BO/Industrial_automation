// Programa Elevador de Carga - Con Home y Recuperación
// Arduino PLC Opta - 8 entradas, 4 salidas

// Definición de entradas
const int BOTON_SUBIR_PB = I1;      // Botón SUBIR en planta baja
const int BOTON_BAJAR_PB = I2;      // Botón BAJAR en planta baja
const int BOTON_SUBIR_P1 = I3;      // Botón SUBIR en planta alta
const int BOTON_BAJAR_P1 = I4;      // Botón BAJAR en planta alta
const int BOTON_PB_INT = I5;        // Botón interior PB
const int BOTON_P1_INT = I6;        // Botón interior P1
const int FIN_CARRERA_PB = I7;      // Final carrera PB (NA)
const int FIN_CARRERA_P1 = I8;      // Final carrera P1 (NA)

// Definición de salidas
const int MOTOR_SUBIR = D1;         // Control subida polipasto
const int MOTOR_BAJAR = D2;         // Control bajada polipasto
const int LED_PISO_PB = D3;         // Indicador piso PB
const int LED_PISO_P1 = D4;         // Indicador piso P1

// Estados del sistema
enum EstadoSistema {
  NORMAL,           // Operación normal
  POSICION_PERDIDA, // No sabe dónde está
  BUSCANDO_HOME,    // Buscando referencia (PB)
  EMERGENCIA        // Estado de emergencia
};

// Direcciones de movimiento
enum Direccion {
  DETENIDO,
  SUBIENDO,
  BAJANDO
};

// Variables de estado
EstadoSistema estadoSistema = NORMAL;
Direccion direccionActual = DETENIDO;
int pisoActual = -1; // -1 = posición desconocida, 0 = PB, 1 = P1
bool solicitudPB = false;
bool solicitudP1 = false;

// Variables para tiempo
unsigned long tiempoInicioMovimiento = 0;
unsigned long tiempoUltimoEvento = 0;
const unsigned long TIEMPO_MAXIMO_MOVIMIENTO = 30000; // 30 seg máx
const unsigned long TIEMPO_ESPERA_RECUPERACION = 5000; // 5 seg para intentar recuperación

// Variable para debounce
unsigned long ultimoTiempoBoton = 0;
const unsigned long DEBOUNCE_TIME = 50;

// Contador de intentos de recuperación
int intentosRecuperacion = 0;
const int MAX_INTENTOS_RECUPERACION = 3;

void setup() {
  // Configurar entradas
  pinMode(BOTON_SUBIR_PB, INPUT_PULLDOWN);
  pinMode(BOTON_BAJAR_PB, INPUT_PULLDOWN);
  pinMode(BOTON_SUBIR_P1, INPUT_PULLDOWN);
  pinMode(BOTON_BAJAR_P1, INPUT_PULLDOWN);
  pinMode(BOTON_PB_INT, INPUT_PULLDOWN);
  pinMode(BOTON_P1_INT, INPUT_PULLDOWN);
  pinMode(FIN_CARRERA_PB, INPUT_PULLDOWN);
  pinMode(FIN_CARRERA_P1, INPUT_PULLDOWN);
  
  // Configurar salidas
  pinMode(MOTOR_SUBIR, OUTPUT);
  pinMode(MOTOR_BAJAR, OUTPUT);
  pinMode(LED_PISO_PB, OUTPUT);
  pinMode(LED_PISO_P1, OUTPUT);
  
  // Estado inicial
  digitalWrite(MOTOR_SUBIR, LOW);
  digitalWrite(MOTOR_BAJAR, LOW);
  
  // Verificar posición inicial
  verificarPosicionInicial();
  
  Serial.begin(9600);
  Serial.println("Elevador de Carga - Con Home y Recuperación");
  imprimirEstado();
}

void loop() {
  // Actualizar posición basado en fines de carrera
  actualizarPosicion();
  
  // Verificar si perdió la posición
  verificarPerdidaPosicion();
  
  // Procesar según estado del sistema
  switch(estadoSistema) {
    case NORMAL:
      operacionNormal();
      break;
      
    case POSICION_PERDIDA:
      manejarPosicionPerdida();
      break;
      
    case BUSCANDO_HOME:
      buscarHome();
      break;
      
    case EMERGENCIA:
      manejarEmergencia();
      break;
  }
  
  // Actualizar indicadores siempre
  actualizarIndicadores();
  
  delay(20);
}

void verificarPosicionInicial() {
  bool enPB = digitalRead(FIN_CARRERA_PB);
  bool enP1 = digitalRead(FIN_CARRERA_P1);
  
  if (enPB) {
    pisoActual = 0;
    estadoSistema = NORMAL;
    Serial.println("Posición inicial: PLANTA BAJA");
  }
  else if (enP1) {
    pisoActual = 1;
    estadoSistema = NORMAL;
    Serial.println("Posición inicial: PLANTA ALTA");
  }
  else {
    pisoActual = -1;
    estadoSistema = POSICION_PERDIDA;
    Serial.println("!!! POSICIÓN DESCONOCIDA - Buscando HOME (PB) !!!");
  }
}

void actualizarPosicion() {
  bool enPB = digitalRead(FIN_CARRERA_PB);
  bool enP1 = digitalRead(FIN_CARRERA_P1);
  
  // Solo actualizar si estamos en NORMAL o BUSCANDO_HOME
  if (estadoSistema == NORMAL || estadoSistema == BUSCANDO_HOME) {
    if (enPB) {
      pisoActual = 0;
      detenerMotor();
      solicitudPB = false;
      solicitudP1 = false;
      if (estadoSistema == BUSCANDO_HOME) {
        estadoSistema = NORMAL;
        intentosRecuperacion = 0;
        Serial.println("HOME encontrado - Sistema recuperado");
      }
      tiempoUltimoEvento = millis();
    }
    else if (enP1) {
      pisoActual = 1;
      detenerMotor();
      solicitudPB = false;
      solicitudP1 = false;
      tiempoUltimoEvento = millis();
    }
  }
}

void verificarPerdidaPosicion() {
  // Solo verificar en estado NORMAL
  if (estadoSistema != NORMAL) return;
  
  bool enPB = digitalRead(FIN_CARRERA_PB);
  bool enP1 = digitalRead(FIN_CARRERA_P1);
  
  // Si no está en ningún piso pero debería estarlo (por movimiento previo)
  if (!enPB && !enP1) {
    // Si pasó demasiado tiempo sin detectar posición
    if (millis() - tiempoUltimoEvento > TIEMPO_MAXIMO_MOVIMIENTO + 5000) {
      pisoActual = -1;
      estadoSistema = POSICION_PERDIDA;
      Serial.println("!!! POSICIÓN PERDIDA - Tiempo de movimiento excedido !!!");
    }
  } else {
    tiempoUltimoEvento = millis();
  }
}

void operacionNormal() {
  // Procesar botones solo en estado normal
  procesarBotones();
  
  // Control de movimiento normal
  if (pisoActual != -1) {
    if (solicitudPB && pisoActual == 1) {
      iniciarMovimiento(BAJANDO);
    }
    else if (solicitudP1 && pisoActual == 0) {
      iniciarMovimiento(SUBIENDO);
    }
    else if (!solicitudPB && !solicitudP1) {
      detenerMotor();
    }
  }
  
  // Ejecutar movimiento según dirección
  ejecutarMovimiento();
  
  // Verificar tiempo máximo
  verificarTiempoMovimiento();
}

void manejarPosicionPerdida() {
  // Detener cualquier movimiento
  detenerMotor();
  
  // Parpadear LEDs para indicar error
  parpadearLEDs(500); // Parpadeo lento
  
  // Esperar un momento antes de intentar recuperación
  static unsigned long tiempoInicioPerdida = 0;
  if (tiempoInicioPerdida == 0) {
    tiempoInicioPerdida = millis();
  }
  
  // Después de 5 segundos, intentar buscar home
  if (millis() - tiempoInicioPerdida > TIEMPO_ESPERA_RECUPERACION) {
    if (intentosRecuperacion < MAX_INTENTOS_RECUPERACION) {
      estadoSistema = BUSCANDO_HOME;
      intentosRecuperacion++;
      tiempoInicioPerdida = 0;
      Serial.print("Intento de recuperación #");
      Serial.println(intentosRecuperacion);
    } else {
      // Demasiados intentos fallidos - pasar a emergencia
      estadoSistema = EMERGENCIA;
      tiempoInicioPerdida = 0;
      Serial.println("!!! MÚLTIPLES FALLOS - ESTADO DE EMERGENCIA !!!");
    }
  }
}

void buscarHome() {
  // Estrategia: Bajar lentamente hasta encontrar PB (home)
  static unsigned long tiempoBusqueda = 0;
  static bool buscando = false;
  
  // Activar indicador visual de búsqueda
  digitalWrite(LED_PISO_PB, !digitalRead(LED_PISO_PB));
  delay(100); // Efecto visual
  
  bool enPB = digitalRead(FIN_CARRERA_PB);
  bool enP1 = digitalRead(FIN_CARRERA_P1);
  
  if (!buscando && !enPB) {
    // Iniciar búsqueda - bajar lentamente
    Serial.println("Buscando HOME - Bajando lentamente");
    digitalWrite(MOTOR_SUBIR, LOW);
    digitalWrite(MOTOR_BAJAR, HIGH); // Bajar
    buscando = true;
    tiempoBusqueda = millis();
  }
  
  // Verificar si encontramos PB
  if (enPB) {
    detenerMotor();
    pisoActual = 0;
    estadoSistema = NORMAL;
    intentosRecuperacion = 0;
    buscando = false;
    Serial.println("HOME encontrado - Sistema recuperado");
  }
  
  // Verificar tiempo máximo de búsqueda
  if (buscando && millis() - tiempoBusqueda > 10000) { // 10 seg máx
    detenerMotor();
    buscando = false;
    estadoSistema = POSICION_PERDIDA;
    Serial.println("Tiempo de búsqueda excedido - Reintentando");
  }
  
  // Seguridad: si detectamos P1 durante la búsqueda, paramos
  if (enP1 && buscando) {
    detenerMotor();
    buscando = false;
    estadoSistema = POSICION_PERDIDA;
    Serial.println("Detectado P1 durante búsqueda - Estrategia alternativa");
  }
}

void manejarEmergencia() {
  // Detener todo
  detenerMotor();
  
  // Parpadear LEDs rápidamente
  parpadearLEDs(200); // Parpadeo rápido
  
  // Esperar intervención manual o reset
  static unsigned long tiempoEmergencia = 0;
  if (tiempoEmergencia == 0) {
    tiempoEmergencia = millis();
    Serial.println("!!! MODO EMERGENCIA - Requiere intervención manual !!!");
  }
  
  // Intentar reset automático después de 30 seg (opcional)
  if (millis() - tiempoEmergencia > 30000) {
    // Verificar si podemos resetear
    bool enPB = digitalRead(FIN_CARRERA_PB);
    bool enP1 = digitalRead(FIN_CARRERA_P1);
    
    if (enPB || enP1) {
      estadoSistema = NORMAL;
      intentosRecuperacion = 0;
      tiempoEmergencia = 0;
      Serial.println("Reset automático de emergencia - Sistema normal");
    }
  }
}

void procesarBotones() {
  if (millis() - ultimoTiempoBoton > DEBOUNCE_TIME) {
    
    // Solo procesar solicitudes si estamos en estado normal
    if (estadoSistema == NORMAL) {
      // Solicitudes a Planta Baja
      if (digitalRead(BOTON_SUBIR_PB) == HIGH && pisoActual != 0) {
        solicitudPB = true;
        Serial.println("Solicitud: Llamar a PB");
        ultimoTiempoBoton = millis();
      }
      else if (digitalRead(BOTON_PB_INT) == HIGH && pisoActual != 0) {
        solicitudPB = true;
        Serial.println("Solicitud interior: Ir a PB");
        ultimoTiempoBoton = millis();
      }
      else if (digitalRead(BOTON_BAJAR_P1) == HIGH && pisoActual == 1) {
        solicitudPB = true;
        Serial.println("Solicitud desde P1: Bajar a PB");
        ultimoTiempoBoton = millis();
      }
      
      // Solicitudes a Planta Alta
      if (digitalRead(BOTON_SUBIR_P1) == HIGH && pisoActual != 1) {
        solicitudP1 = true;
        Serial.println("Solicitud: Llamar a P1");
        ultimoTiempoBoton = millis();
      }
      else if (digitalRead(BOTON_P1_INT) == HIGH && pisoActual != 1) {
        solicitudP1 = true;
        Serial.println("Solicitud interior: Ir a P1");
        ultimoTiempoBoton = millis();
      }
      else if (digitalRead(BOTON_BAJAR_PB) == HIGH && pisoActual == 0) {
        solicitudP1 = true;
        Serial.println("Solicitud desde PB: Subir a P1");
        ultimoTiempoBoton = millis();
      }
    }
    
    // Botón de emergencia virtual (opcional - podría ser una combinación)
    if (digitalRead(BOTON_SUBIR_PB) == HIGH && digitalRead(BOTON_BAJAR_PB) == HIGH) {
      estadoSistema = EMERGENCIA;
      Serial.println("Emergencia activada por botones");
      ultimoTiempoBoton = millis();
    }
  }
}

void ejecutarMovimiento() {
  switch(direccionActual) {
    case SUBIENDO:
      if (!digitalRead(FIN_CARRERA_P1)) {
        digitalWrite(MOTOR_SUBIR, HIGH);
        digitalWrite(MOTOR_BAJAR, LOW);
      } else {
        detenerMotor();
      }
      break;
      
    case BAJANDO:
      if (!digitalRead(FIN_CARRERA_PB)) {
        digitalWrite(MOTOR_SUBIR, LOW);
        digitalWrite(MOTOR_BAJAR, HIGH);
      } else {
        detenerMotor();
      }
      break;
      
    case DETENIDO:
      detenerMotor();
      break;
  }
}

void iniciarMovimiento(Direccion dir) {
  if (direccionActual == DETENIDO && estadoSistema == NORMAL) {
    direccionActual = dir;
    tiempoInicioMovimiento = millis();
    Serial.print("Iniciando: ");
    Serial.println(dir == SUBIENDO ? "SUBIR" : "BAJAR");
  }
}

void detenerMotor() {
  digitalWrite(MOTOR_SUBIR, LOW);
  digitalWrite(MOTOR_BAJAR, LOW);
  direccionActual = DETENIDO;
}

void actualizarIndicadores() {
  // Indicadores de piso
  if (pisoActual == 0) {
    digitalWrite(LED_PISO_PB, HIGH);
    digitalWrite(LED_PISO_P1, LOW);
  }
  else if (pisoActual == 1) {
    digitalWrite(LED_PISO_PB, LOW);
    digitalWrite(LED_PISO_P1, HIGH);
  }
  else {
    // Posición desconocida - ambos LEDs pueden usarse para indicar error
    if (estadoSistema == BUSCANDO_HOME) {
      // Ya manejamos el parpadeo en buscarHome()
    } else if (estadoSistema == EMERGENCIA) {
      // Ya manejamos el parpadeo en manejarEmergencia()
    }
  }
}

void parpadearLEDs(int intervalo) {
  static unsigned long tiempoAnterior = 0;
  static bool estadoLED = false;
  
  if (millis() - tiempoAnterior > intervalo) {
    estadoLED = !estadoLED;
    digitalWrite(LED_PISO_PB, estadoLED);
    digitalWrite(LED_PISO_P1, estadoLED);
    tiempoAnterior = millis();
  }
}

void verificarTiempoMovimiento() {
  if (direccionActual != DETENIDO && estadoSistema == NORMAL) {
    if (millis() - tiempoInicioMovimiento > TIEMPO_MAXIMO_MOVIMIENTO) {
      // Tiempo excedido - posible pérdida de posición
      detenerMotor();
      pisoActual = -1;
      estadoSistema = POSICION_PERDIDA;
      Serial.println("!!! TIEMPO MÁXIMO EXCEDIDO - POSIBLE PÉRDIDA !!!");
    }
  }
}

void imprimirEstado() {
  Serial.print("Estado: ");
  switch(estadoSistema) {
    case NORMAL: Serial.print("NORMAL"); break;
    case POSICION_PERDIDA: Serial.print("POSICIÓN PERDIDA"); break;
    case BUSCANDO_HOME: Serial.print("BUSCANDO HOME"); break;
    case EMERGENCIA: Serial.print("EMERGENCIA"); break;
  }
  
  Serial.print(" | Piso: ");
  if (pisoActual == 0) Serial.print("PB");
  else if (pisoActual == 1) Serial.print("P1");
  else Serial.print("???");
  
  Serial.print(" | Intentos: ");
  Serial.println(intentosRecuperacion);
}
