// Programa Elevador de Carga - Versión Simplificada
// Arduino PLC Opta - 8 entradas, 4 salidas
// Enfocado en seguridad y funcionalidad básica

// Definición de entradas
const int BOTON_SUBIR_PB = I1;      // Botón SUBIR en planta baja
const int BOTON_BAJAR_PB = I2;      // Botón BAJAR en planta baja
const int BOTON_SUBIR_P1 = I3;      // Botón SUBIR en planta alta
const int BOTON_BAJAR_P1 = I4;      // Botón BAJAR en planta alta
const int FIN_CARRERA_PB = I5;      // Final carrera PB (NA)
const int FIN_CARRERA_P1 = I6;      // Final carrera P1 (NA)
const int INTERLOCKS = I7;           // Interlocks puertas (0 = puertas cerradas, 1 = alguna abierta)
const int PARO_EMERGENCIA = I8;      // Paro de emergencia (0 = normal, 1 = emergencia)

// Definición de salidas
const int MOTOR_SUBIR = Q0;          // Control subida poliposto
const int MOTOR_BAJAR = Q1;          // Control bajada poliposto
const int LED_PISO = Q2;             // Indicador piso (ON = PB, OFF = P1, Parpadeo = error)
const int ALARMA = Q3;                // Alarma de paro o interlocks

// Estados del sistema
enum EstadoSistema {
  NORMAL,
  EMERGENCIA,
  POSICION_PERDIDA,
  BUSCANDO_HOME
};

enum Direccion {
  DETENIDO,
  SUBIENDO,
  BAJANDO
};

// Variables de estado
EstadoSistema estadoSistema = NORMAL;
Direccion direccionActual = DETENIDO;
bool pisoPB = false;  // true = estamos en PB, false = estamos en P1
bool posicionConocida = false;

// Variables para solicitudes
bool solicitudPB = false;
bool solicitudP1 = false;

// Variables de tiempo
unsigned long tiempoInicioMovimiento = 0;
unsigned long tiempoUltimoEvento = 0;
unsigned long tiempoAlarma = 0;
const unsigned long TIEMPO_MAXIMO_MOVIMIENTO = 30000; // 30 segundos
const unsigned long TIEMPO_ESPERA_RECUPERACION = 5000; // 5 segundos

// Variables para debounce
unsigned long ultimoTiempoBoton = 0;
const unsigned long DEBOUNCE_TIME = 50;

void setup() {
  // Configurar entradas
  pinMode(BOTON_SUBIR_PB, INPUT_PULLDOWN);
  pinMode(BOTON_BAJAR_PB, INPUT_PULLDOWN);
  pinMode(BOTON_SUBIR_P1, INPUT_PULLDOWN);
  pinMode(BOTON_BAJAR_P1, INPUT_PULLDOWN);
  pinMode(FIN_CARRERA_PB, INPUT_PULLDOWN);
  pinMode(FIN_CARRERA_P1, INPUT_PULLDOWN);
  pinMode(INTERLOCKS, INPUT_PULLDOWN);
  pinMode(PARO_EMERGENCIA, INPUT_PULLDOWN);
  
  // Configurar salidas
  pinMode(MOTOR_SUBIR, OUTPUT);
  pinMode(MOTOR_BAJAR, OUTPUT);
  pinMode(LED_PISO, OUTPUT);
  pinMode(ALARMA, OUTPUT);
  
  // Estado inicial
  digitalWrite(MOTOR_SUBIR, LOW);
  digitalWrite(MOTOR_BAJAR, LOW);
  digitalWrite(ALARMA, LOW);
  
  // Verificar posición inicial
  verificarPosicionInicial();
  
  Serial.begin(9600);
  Serial.println("Elevador de Carga - Versión Simplificada");
  Serial.println("Sistema de seguridad integrado");
}

void loop() {
  // 1. VERIFICACIONES DE SEGURIDAD (Prioridad máxima)
  if (verificarSeguridad()) {
    // Si hay condición de seguridad, manejamos emergencia
    manejarEmergencia();
    return;
  }
  
  // 2. ACTUALIZAR POSICIÓN
  actualizarPosicion();
  
  // 3. VERIFICAR PÉRDIDA DE POSICIÓN
  verificarPerdidaPosicion();
  
  // 4. MÁQUINA DE ESTADOS PRINCIPAL
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
  
  // 5. ACTUALIZAR INDICADORES
  actualizarIndicadores();
  
  delay(20); // Pequeña pausa para estabilidad
}

bool verificarSeguridad() {
  // Verificar paro de emergencia
  if (digitalRead(PARO_EMERGENCIA) == HIGH) {
    if (estadoSistema != EMERGENCIA) {
      Serial.println("!!! PARO DE EMERGENCIA ACTIVADO !!!");
      estadoSistema = EMERGENCIA;
    }
    return true;
  }
  
  // Verificar interlocks durante movimiento
  if (direccionActual != DETENIDO && digitalRead(INTERLOCKS) == HIGH) {
    if (estadoSistema != EMERGENCIA) {
      Serial.println("!!! PUERTA ABIERTA DURANTE MOVIMIENTO !!!");
      estadoSistema = EMERGENCIA;
    }
    return true;
  }
  
  return false;
}

void verificarPosicionInicial() {
  bool enPB = digitalRead(FIN_CARRERA_PB);
  bool enP1 = digitalRead(FIN_CARRERA_P1);
  
  if (enPB) {
    pisoPB = true;
    posicionConocida = true;
    Serial.println("Posición inicial: PLANTA BAJA");
  }
  else if (enP1) {
    pisoPB = false;
    posicionConocida = true;
    Serial.println("Posición inicial: PLANTA ALTA");
  }
  else {
    posicionConocida = false;
    estadoSistema = POSICION_PERDIDA;
    Serial.println("!!! POSICIÓN DESCONOCIDA - Buscando HOME (PB) !!!");
  }
}

void actualizarPosicion() {
  bool enPB = digitalRead(FIN_CARRERA_PB);
  bool enP1 = digitalRead(FIN_CARRERA_P1);
  
  if (enPB) {
    pisoPB = true;
    posicionConocida = true;
    detenerMovimiento();
    solicitudPB = false;
    solicitudP1 = false;
    if (estadoSistema == BUSCANDO_HOME) {
      estadoSistema = NORMAL;
      Serial.println("HOME encontrado - Sistema normal");
    }
    tiempoUltimoEvento = millis();
  }
  else if (enP1) {
    pisoPB = false;
    posicionConocida = true;
    detenerMovimiento();
    solicitudPB = false;
    solicitudP1 = false;
    tiempoUltimoEvento = millis();
  }
}

void verificarPerdidaPosicion() {
  if (estadoSistema != NORMAL) return;
  
  bool enPB = digitalRead(FIN_CARRERA_PB);
  bool enP1 = digitalRead(FIN_CARRERA_P1);
  
  if (!enPB && !enP1 && posicionConocida) {
    // Estamos en movimiento, está bien
    if (direccionActual == DETENIDO) {
      // Deberíamos estar en un piso pero no lo estamos
      if (millis() - tiempoUltimoEvento > 5000) {
        posicionConocida = false;
        estadoSistema = POSICION_PERDIDA;
        Serial.println("!!! POSICIÓN PERDIDA !!!");
      }
    }
  }
}

void operacionNormal() {
  // Solo operar si tenemos posición conocida y no hay emergencia
  if (!posicionConocida) {
    estadoSistema = POSICION_PERDIDA;
    return;
  }
  
  // Leer botones
  procesarBotones();
  
  // Control de movimiento
  if (direccionActual == DETENIDO) {
    if (solicitudPB && !pisoPB) {
      // Estamos en P1, debemos bajar a PB
      iniciarMovimiento(BAJANDO);
    }
    else if (solicitudP1 && pisoPB) {
      // Estamos en PB, debemos subir a P1
      iniciarMovimiento(SUBIENDO);
    }
  }
  
  // Ejecutar movimiento
  ejecutarMovimiento();
}

void procesarBotones() {
  if (millis() - ultimoTiempoBoton > DEBOUNCE_TIME) {
    
    // Botón SUBIR en PB (llamar a PB)
    if (digitalRead(BOTON_SUBIR_PB) == HIGH && !pisoPB) {
      solicitudPB = true;
      Serial.println("Solicitud: Llamar a PB");
      ultimoTiempoBoton = millis();
    }
    
    // Botón BAJAR en PB (enviar a P1)
    if (digitalRead(BOTON_BAJAR_PB) == HIGH && pisoPB) {
      solicitudP1 = true;
      Serial.println("Solicitud desde PB: Subir a P1");
      ultimoTiempoBoton = millis();
    }
    
    // Botón SUBIR en P1 (llamar a P1)
    if (digitalRead(BOTON_SUBIR_P1) == HIGH && pisoPB) {
      solicitudP1 = true;
      Serial.println("Solicitud: Llamar a P1");
      ultimoTiempoBoton = millis();
    }
    
    // Botón BAJAR en P1 (enviar a PB)
    if (digitalRead(BOTON_BAJAR_P1) == HIGH && !pisoPB) {
      solicitudPB = true;
      Serial.println("Solicitud desde P1: Bajar a PB");
      ultimoTiempoBoton = millis();
    }
  }
}

void ejecutarMovimiento() {
  // Verificar interlocks antes de mover
  if (digitalRead(INTERLOCKS) == HIGH) {
    detenerMovimiento();
    return;
  }
  
  switch(direccionActual) {
    case SUBIENDO:
      if (!digitalRead(FIN_CARRERA_P1)) {
        digitalWrite(MOTOR_SUBIR, HIGH);
        digitalWrite(MOTOR_BAJAR, LOW);
      } else {
        detenerMovimiento();
      }
      break;
      
    case BAJANDO:
      if (!digitalRead(FIN_CARRERA_PB)) {
        digitalWrite(MOTOR_SUBIR, LOW);
        digitalWrite(MOTOR_BAJAR, HIGH);
      } else {
        detenerMovimiento();
      }
      break;
      
    case DETENIDO:
      digitalWrite(MOTOR_SUBIR, LOW);
      digitalWrite(MOTOR_BAJAR, LOW);
      break;
  }
  
  // Verificar tiempo máximo de movimiento
  if (direccionActual != DETENIDO) {
    if (millis() - tiempoInicioMovimiento > TIEMPO_MAXIMO_MOVIMIENTO) {
      detenerMovimiento();
      posicionConocida = false;
      estadoSistema = POSICION_PERDIDA;
      Serial.println("!!! TIEMPO MÁXIMO EXCEDIDO !!!");
    }
  }
}

void iniciarMovimiento(Direccion dir) {
  if (direccionActual == DETENIDO && digitalRead(INTERLOCKS) == LOW) {
    direccionActual = dir;
    tiempoInicioMovimiento = millis();
    Serial.print("Iniciando movimiento: ");
    Serial.println(dir == SUBIENDO ? "SUBIR" : "BAJAR");
  }
}

void detenerMovimiento() {
  direccionActual = DETENIDO;
  digitalWrite(MOTOR_SUBIR, LOW);
  digitalWrite(MOTOR_BAJAR, LOW);
}

void manejarPosicionPerdida() {
  // Detener movimiento
  detenerMovimiento();
  
  // Indicar error con LED (parpadeo lento)
  static unsigned long tiempoParpadeo = 0;
  static bool estadoLED = false;
  
  if (millis() - tiempoParpadeo > 1000) {
    estadoLED = !estadoLED;
    digitalWrite(LED_PISO, estadoLED);
    tiempoParpadeo = millis();
  }
  
  // Intentar recuperación
  static unsigned long tiempoInicioPerdida = 0;
  if (tiempoInicioPerdida == 0) {
    tiempoInicioPerdida = millis();
  }
  
  if (millis() - tiempoInicioPerdida > TIEMPO_ESPERA_RECUPERACION) {
    estadoSistema = BUSCANDO_HOME;
    tiempoInicioPerdida = 0;
    Serial.println("Iniciando búsqueda de HOME");
  }
}

void buscarHome() {
  // Estrategia: bajar hasta encontrar PB
  static bool buscando = false;
  static unsigned long tiempoBusqueda = 0;
  
  if (!buscando && !digitalRead(FIN_CARRERA_PB)) {
    // Iniciar búsqueda hacia abajo
    Serial.println("Buscando HOME - Bajando");
    digitalWrite(MOTOR_SUBIR, LOW);
    digitalWrite(MOTOR_BAJAR, HIGH);
    buscando = true;
    tiempoBusqueda = millis();
  }
  
  // Verificar si encontramos PB
  if (digitalRead(FIN_CARRERA_PB)) {
    detenerMovimiento();
    buscando = false;
    // La posición se actualizará en actualizarPosicion()
  }
  
  // Verificar tiempo máximo de búsqueda
  if (buscando && millis() - tiempoBusqueda > 10000) {
    detenerMovimiento();
    buscando = false;
    estadoSistema = POSICION_PERDIDA;
    Serial.println("Búsqueda fallida - Reintentando");
  }
}

void manejarEmergencia() {
  // Detener todo inmediatamente
  detenerMovimiento();
  
  // Activar alarma
  digitalWrite(ALARMA, HIGH);
  
  // Indicar emergencia con LED (parpadeo rápido)
  static unsigned long tiempoParpadeo = 0;
  static bool estadoLED = false;
  
  if (millis() - tiempoParpadeo > 200) {
    estadoLED = !estadoLED;
    digitalWrite(LED_PISO, estadoLED);
    tiempoParpadeo = millis();
  }
  
  // Verificar si se puede salir de emergencia
  static unsigned long tiempoEmergencia = 0;
  if (tiempoEmergencia == 0) {
    tiempoEmergencia = millis();
  }
  
  // Condiciones para salir de emergencia:
  // - Paro de emergencia desactivado
  // - Interlocks cerrados
  // - Tiempo mínimo de emergencia cumplido
  if (digitalRead(PARO_EMERGENCIA) == LOW && 
      digitalRead(INTERLOCKS) == LOW &&
      millis() - tiempoEmergencia > 3000) {
    
    digitalWrite(ALARMA, LOW);
    estadoSistema = NORMAL;
    tiempoEmergencia = 0;
    Serial.println("Emergencia resuelta - Sistema normal");
  }
}

void actualizarIndicadores() {
  // Solo actualizar si no estamos en emergencia o pérdida de posición
  if (estadoSistema == NORMAL) {
    if (posicionConocida) {
      // LED ON = PB, LED OFF = P1
      digitalWrite(LED_PISO, pisoPB ? HIGH : LOW);
    }
  }
}