// Déclaration des capteurs
const int sensorPins[4] = {A0, A1, A2, A3}; // Capteurs branchés sur les entrées analogiques

// Positions des capteurs en cm (X, Y)
const float sensorPos[4][2] = {
  {27, 34}, // Capteur 1
  {0, 0},   // Capteur 2
  {27, 0},  // Capteur 3
  {0, 34}   // Capteur 4
};

const float speedOfWave = 34000.0; // Vitesse du son en cm/s (dans l'air)

unsigned long arrivalTimes[4]; // Temps d'arrivée pour chaque capteur

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 4; i++) {
    pinMode(sensorPins[i], INPUT);
  }
}

void loop() {
  bool impactDetected = false;

  // Détecter une vibration
  for (int i = 0; i < 4; i++) {
    int sensorValue = analogRead(sensorPins[i]);
    if (sensorValue > 50) {
      impactDetected = true;
      break;
    }
  }

  if (impactDetected) {
    captureArrivalTimes();
    localizeImpact();
    delay(500); // Pause pour éviter plusieurs détections successives
  }
}

void captureArrivalTimes() {
  unsigned long startTime = micros();
  bool detected[4] = {false, false, false, false};

  while (true) {
    bool allDetected = true;
    for (int i = 0; i < 4; i++) {
      if (!detected[i]) {
        int sensorValue = analogRead(sensorPins[i]);
        if (sensorValue > 50) {
          arrivalTimes[i] = micros() - startTime;
          detected[i] = true;
        } else {
          allDetected = false;
        }
      }
    }
    if (allDetected) break;

    if (micros() - startTime > 100000) { // Timeout 100 ms
      for (int i = 0; i < 4; i++) {
        if (!detected[i]) {
          arrivalTimes[i] = 100000;
        }
      }
      break;
    }
  }
}

void localizeImpact() {
  int refSensor = 0;
  unsigned long minTime = arrivalTimes[0];
  for (int i = 1; i < 4; i++) {
    if (arrivalTimes[i] < minTime) {
      minTime = arrivalTimes[i];
      refSensor = i;
    }
  }

  float t_ref = arrivalTimes[refSensor] / 1000000.0; // µs → s
  float tdoa[4];

  Serial.println("TDOA (en secondes) :");
  for (int i = 0; i < 4; i++) {
    tdoa[i] = (arrivalTimes[i] - arrivalTimes[refSensor]) / 1000000.0;
    Serial.print("Capteur ");
    Serial.print(i + 1);
    Serial.print(" : ");
    Serial.println(tdoa[i], 6);
  }

  // Multilatération
  float A[2][2];
  float b[2];
  int idx = 0;

  for (int i = 0; i < 4; i++) {
    if (i != refSensor && idx < 2) {
      float xi = sensorPos[i][0];
      float yi = sensorPos[i][1];
      float x0 = sensorPos[refSensor][0];
      float y0 = sensorPos[refSensor][1];

      A[idx][0] = xi - x0;
      A[idx][1] = yi - y0;
      b[idx] = 0.5 * ((xi * xi + yi * yi) - (x0 * x0 + y0 * y0) - pow(speedOfWave * tdoa[i], 2));
      idx++;
    }
  }

  float det = A[0][0] * A[1][1] - A[0][1] * A[1][0];
  if (abs(det) < 1e-6) {
    Serial.println("Erreur : déterminant nul.");
    return;
  }

  float invA[2][2];
  invA[0][0] =  A[1][1] / det;
  invA[0][1] = -A[0][1] / det;
  invA[1][0] = -A[1][0] / det;
  invA[1][1] =  A[0][0] / det;

  float x = invA[0][0] * b[0] + invA[0][1] * b[1];
  float y = invA[1][0] * b[0] + invA[1][1] * b[1];

  // Correction forcée à l’intérieur de la plaque
  x = constrain(x, 0, 27);
  y = constrain(y, 0, 34);

  Serial.println("✅ Balle détectée !");
  Serial.print("Position estimée : X = ");
  Serial.print(x, 2);
  Serial.print(" cm , Y = ");
  Serial.print(y, 2);
  Serial.println(" cm");
}
