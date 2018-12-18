#include <Arduino.h>
#include <thermistor.h>
#include "BasicStepperDriver.h"
 
// Motor steps per revolution. Most steppers are 200 steps or 1.8 degrees/step
#define MOTOR_STEPS 200
#define RPM 50000
 
// Since microstepping is set externally, make sure this matches the selected mode
// If it doesn't, the motor will move at a different RPM than chosen
// 1=full step, 2=half step etc.
#define MICROSTEPS 1
 
//Y = magazijn 2
//E = magazijn 1
//X = stift
//Q = vork

//Color1 = Kleur Sensor VOOR stift
//Color2 = Kleur Sensor NA stift

#define X_STEP_PIN         54
#define X_DIR_PIN          55
#define X_ENABLE_PIN       38

#define Y_STEP_PIN         60
#define Y_DIR_PIN          61
#define Y_ENABLE_PIN       56

#define E_STEP_PIN         26
#define E_DIR_PIN          28
#define E_ENABLE_PIN       24

#define Q_STEP_PIN         36
#define Q_DIR_PIN          34
#define Q_ENABLE_PIN       30

#define Color1S0           29 //29
#define Color1S1           31 //31
#define Color1S2           35 //35
#define Color1S3           33 //33
#define Color1SensorOut    37 //37

#define Color2S0           16
#define Color2S1           17
#define Color2S2           23
#define Color2S3           25
#define Color2SensorOut    27

#define InductiveDeny      0
#define InductiveAccept    1

#define switchPin          14
#define switchPinPusher1   19
#define switchPinPusher2   18
#define switchPinSolenoid  15
#define switchPinStock2    2
#define switchPinStock1    3

#define solenoidPin        9
#define heaterPin          10
//#define heatSensorPin      A13

thermistor tempSensor(A13,0);

BasicStepperDriver forkMotor(MOTOR_STEPS, Q_DIR_PIN, Q_STEP_PIN, Q_ENABLE_PIN);
BasicStepperDriver stock1Motor(MOTOR_STEPS, E_DIR_PIN, E_STEP_PIN, E_ENABLE_PIN);
BasicStepperDriver stock2Motor(MOTOR_STEPS, Y_DIR_PIN, Y_STEP_PIN, Y_ENABLE_PIN);
BasicStepperDriver markerMotor(MOTOR_STEPS, X_DIR_PIN, X_STEP_PIN, X_ENABLE_PIN);

//Values for color sensoring
int frequency = 0;
int currentR = 0, currentG = 0, currentB = 0;

//Values for main process
bool isBusy = false;
bool initiallyCalibrated = false;
bool markerFinished = false;
bool blockFinished = false;
bool blockValid = false;

//Values for calibration
bool pusher1Calibrated = false, pusher2Calibrated = false, pusherMCalibrated = false, stopMotor = false; 
bool blockPushed = false;

//Values for instructions configuration and statistics
int amountProduced = 0, amountToProduce = 0, incorrectProduced = 0, linesDrawn = 0;
bool getBlue = false;
bool doDrawPlus = true;

//Values for time tracking
double startTime = 0, endTime = 0, editTime = 0;

//Values for positioning of fork
enum ForkPosition {
  UNKNOWNPOS,
  HOME,
  STOCK1,
  STOCK2,
  COLORSENSOR1,
  MARKER,
  COLORSENSOR2,
  END
};

ForkPosition latestPosition = UNKNOWNPOS;

void setup() {
    Serial.begin(9600);

    pinMode(heaterPin, OUTPUT);
    //pinMode(heatSensorPin, INPUT);
    
    pinMode(switchPin, INPUT_PULLUP);
    pinMode(switchPinPusher1, INPUT_PULLUP);
    pinMode(switchPinPusher2, INPUT_PULLUP);
    pinMode(switchPinSolenoid, INPUT_PULLUP);
    pinMode(switchPinStock1, INPUT_PULLUP);
    pinMode(switchPinStock2, INPUT_PULLUP);
 
    forkMotor.begin(RPM, MICROSTEPS);
    stock1Motor.begin(RPM, MICROSTEPS);
    stock2Motor.begin(RPM, MICROSTEPS);
    markerMotor.begin(RPM, MICROSTEPS);

    pinMode(solenoidPin, OUTPUT);

    pinMode(Color1S0, OUTPUT);
    pinMode(Color1S1, OUTPUT);
    pinMode(Color1S2, OUTPUT);
    pinMode(Color1S3, OUTPUT);
    pinMode(Color1SensorOut, INPUT);
  
    digitalWrite(Color1S0, HIGH);
    digitalWrite(Color1S1, LOW);

    pinMode(Color2S0, OUTPUT);
    pinMode(Color2S1, OUTPUT);
    pinMode(Color2S2, OUTPUT);
    pinMode(Color2S3, OUTPUT);
    pinMode(Color2SensorOut, INPUT);
  
    digitalWrite(Color2S0, HIGH);
    digitalWrite(Color2S1, LOW);

    digitalWrite(solenoidPin, HIGH);
}

void loop() {
  //If MIA just booted up, all stepper motors need to be calibrated once.
  if(initiallyCalibrated == false){
    checkEndStopSwitches();
    if(calibrateStepperMotors()){
      return;
    }
    else{
      initiallyCalibrated = true;
    }
  }

  //If MIA is not busy, set new instructions.
  if(isBusy == false){
    //Read from ESP serial port for new operation
    Serial.println("Waiting for new instructions..");
    delay(10000);
    
    //If the next operations is not "null" it should take the instructions and use them for the new production.
    amountToProduce = 1;
    onNewInstructionsFound(true, false);

    isBusy = true;
  }

  //Triggers if MIA is done with current instructions at this point.
  if(amountProduced == amountToProduce){

    //Send complete report to API.
    onInstructionsFinished();
  
    //Also reset production statistics.
    resetProductionStats();

    //Let the system know that there are no instuctions being processed anymore.
    isBusy = false;
    
    return;
  }

  //As long as the amount to produce hasn't been reached, a new block needs to be made.
  if(amountProduced < amountToProduce){
    makeBlockProduct(true, false);
  }
}

void makeBlockProduct(bool blue, bool drawPlus){
    //Set the global configuration values based on the ones given in the method header.
    getBlue = blue;
    doDrawPlus = drawPlus;

    if((pusher2Calibrated == true || pusher1Calibrated == true) && blockPushed == true && blockFinished == true){
      blockPushed = false;
    }

    //Triggered when the method is called when the operation is finished.
    //Resets values to their default state + increases the correctly produced amount by 1.
    if(blockFinished){
      markerFinished = false;
      blockFinished = false;
      blockValid = false;
    }

    //Check if switches have been pressed, if so: update the calibration booleans.
    checkEndStopSwitches();

    //Calibrate stepper motors, if the calibration pocess is not done yet the method will return true
    //If the method returns true: return the method so that the rest of the process is skipped.
    if(calibrateStepperMotors()){
      return;
    }

    //These checks are triggered when the fork is at the home position.
    if(latestPosition == HOME) {
      //Always send the current temperature of the motor to the API
      //regardless of the fork being here to finish or start a new edit.
      sendMotorTemp();
      
      //If the start time is higher than 0 it means that this is the end of a(n) (failed) edit.
      //In that case print the edit time and reset the start and end time.
      if(startTime > 0) {
        if(blockValid){
          amountProduced++;
        }
        else {
          incorrectProduced++;
        }

        //Send last edit data to API
        onEditEnded(blockValid);
      }

      //If the last edit was valid, the blockFinished boolean needs to be true, then end the method.
      if(blockValid == true){
        blockFinished = true;
        return;
      }

      //This is a check to see if the amount to produce has been reached already.
      //True if finished, which ends the method.
      if(amountProduced == amountToProduce){
        return;
      }

      //If the start time is 0 this means that there is a new operation about to start.
      //Set the start time if true.
      if(startTime == 0){
        startTime = millis();
        Serial.println("Set start time.");
      }

      //Based on the color setting, this will make the fork move to first or second stock.
      moveToStock(getBlue? 2 : 1);
    }

    //Check to see if fork arrived at one of the stocks.
    //Returns true if that is the case.
    if(latestPosition == STOCK1 || latestPosition == STOCK2){
      
      //Checks if there has already been a block pushed to the fork.
      //Basically a check to prevent the pusher motors to end in infinite loop.
      if(blockPushed == false){
        
        //Push the block to fork.
        pushBlockToFork();

        //Sets the calibration boolean to false form correct pusher.
        //This is required in order to get the pusher to calibrate again.
        if(getBlue){
          pusher2Calibrated = false;
        }
        else {
          pusher1Calibrated = false;
        }

        //Set the blockPushed boolean to true to make sure that this code only runs once per edit.
        blockPushed = true;
        return;
      }

      //Check if the stock amount is high enough, if not: send a message to Isah API.
      verifyStockAmount(getBlue ? 2 : 1);

      //Makes the fork move to first color sensor based on the current position.
      goToFirstCheck();
    }

    //If the fork arrived at the first sensor this code will be triggered which executes the first check.
    if(latestPosition == COLORSENSOR1){
      scanColorAndAct();
    }

    //If the fork arrived at the marker this code will be executed.
    if(latestPosition == MARKER){
      if(markerFinished == false){
        executeMarkingBlock(doDrawPlus);
        pusherMCalibrated = false;
        markerFinished = true;
      }

      //Before the fork can go to the second check, the marker needs to be calibrated again.
      if(pusherMCalibrated == false){
        return;
      }
      
      goToSecondCheck();
    }
      
    //If the fork arrived at the second sensor this code will be triggered which executes the second check.
    if(latestPosition == COLORSENSOR2){
      checkEditAndAct();
    }

    //If the fork arrived at the end this will be triggered and will make the fork calibrate again.
    if(latestPosition == END){
      returnToHome();
    }
}

bool calibrateStepperMotors(){
    if(pusherMCalibrated == false){
      calibratePusherM();
      return true;
    }
    if(pusher2Calibrated == false){
      calibratePusher2();
      return true;
    }

    if(pusher1Calibrated == false){
      calibratePusher1();
      return true;
    }

    if(latestPosition == UNKNOWNPOS){
      returnToHome();
      return true;
    }

    return false;
}

//Reads the output from the endstop switches.
//If its reading a "1" it sets the correct calibration boolean to true.
//This indicates that the motor has been calibrated.
void checkEndStopSwitches(){
    if(pusherMCalibrated == false && digitalRead(switchPinSolenoid) == 1){
      Serial.println("Calibrated marker!");
      pusherMCalibrated = true;
    }
    if(stopMotor == false && digitalRead(switchPin) == 1){
      Serial.println("Calibrated fork!");
      stopMotor = true;
    }
    if(pusher2Calibrated == false && digitalRead(switchPinPusher2) == 1){
      Serial.println("Calibrated pusher 2!");
      pusher2Calibrated = true;
    }
    if(pusher1Calibrated == false && digitalRead(switchPinPusher1) == 1){
      Serial.println("Calibrated pusher 1!");
      pusher1Calibrated = true;
    }
}

//Reads the output from the stock endstop switches.
//If its reading a "0" it messages the API to notify that he minimum stock has been passed.
void verifyStockAmount(int stockId){
  int pinToRead = stockId == 1 ? switchPinStock1 : stockId == 2 ? switchPinStock2 : -1;
  if(digitalRead(pinToRead) == 0){
    onStockTooLow(stockId);
    //Serial.print("Stock "); Serial.print(stockId); Serial.println(" is out of minimum stock!");
    
    //Send API message to alert users.
    return;
  }
}

void calibratePusher1(){
  if(pusher1Calibrated){
    stock1Motor.disable();
  }
  else {
    stock1Motor.enable();
    stock1Motor.rotate(-32);
  }
}

void calibratePusher2(){
  if(pusher2Calibrated){
    stock2Motor.disable();
  }
  else {
    stock2Motor.enable();
    stock2Motor.rotate(-32);
  }
}

void calibratePusherM(){
  if(pusherMCalibrated){
    markerMotor.disable();
  }
  else {
    markerMotor.enable();
    markerMotor.rotate(-32);
  }
}

void returnToHome(){
  if(stopMotor){
    forkMotor.disable();
    latestPosition = HOME;
  }
  else {
    forkMotor.enable();
    forkMotor.rotate(-32);
  }
}

void executeMarkingBlock(bool drawPlus){
  Serial.println("Initializing marker...");
  initializeMarker();
  
  Serial.println("Marker initialized, commence drawing...");
  digitalWrite(solenoidPin, LOW);

  forkMotor.enable();

  delay(500);
  forkMotor.rotate(2500);
  //Serial.println("Drawing progress: 20%");
  delay(500);
  forkMotor.rotate(-5000);
  //Serial.println("Drawing progress: 40%");
  delay(500);
  forkMotor.rotate(2500);
  //Serial.println("Drawing progress: 60%");
  delay(500);

  linesDrawn++;

  if(drawPlus == true){
    markerMotor.enable();
    
    markerMotor.rotate(15000);
    delay(500);
    markerMotor.rotate(-30000);
    delay(500);

    linesDrawn++;
  }

  sendMotorTemp();

  markerMotor.disable();
  
  digitalWrite(solenoidPin, HIGH);
  Serial.println("Drawing finished! Moving back marker.");
}

void initializeMarker(){
  int rotations = 15750;
  
  markerMotor.enable();
  
  markerMotor.rotate(rotations);
  markerMotor.rotate(rotations);
  markerMotor.rotate(rotations);

  markerMotor.disable();
}

void pushBlockToFork(){
  int rotations = 28500;

  if(getBlue){
    stock2Motor.enable();
  
    stock2Motor.rotate(rotations);
    stock2Motor.rotate(rotations);
    stock2Motor.rotate(rotations);
  
    stock2Motor.disable();
  }
  else {
    stock1Motor.enable();
  
    stock1Motor.rotate(rotations);
    stock1Motor.rotate(rotations);
    stock1Motor.rotate(rotations);
  
    stock1Motor.disable();
  }
  
}

void moveToMarker(){
  stopMotor = false;
  
  forkMotor.enable();
  forkMotor.rotate(17500);
  forkMotor.disable();
  
  latestPosition = MARKER;
}

void scanColorAndAct(){
  int attempts = 0;

  while(blockValid == false){
    if(attempts >= 3){
      if(blockPushed && blockValid == false){
        blockPushed = false;
      }
      return;
    }
    
    executeScan1();

    bool correctColor = getBlue == true ? (currentG >= 110 && currentG <= 132 && currentR > 140 && currentR <= 160 && currentB >= 62 && currentB <= 75)  
                          : (currentG <= 112 && currentR > 145);

    if(correctColor){
      blockValid = true;
      moveToMarker();
    }
    else {
      blockValid = false;
      if(attempts < 2){
      }
      else {
        latestPosition = UNKNOWNPOS;
      }
    }

    resetCurrentRGB();

    attempts++;
  }

}

void checkEditAndAct(){
  int attempts = 0;

  while(attempts < 3){
    if(attempts >= 1 && blockValid == true){
      return;
    }
    executeScan2();

    //Blauw
    //Without: 200 145 40
    //With: 199 148 42
  
    //Groen
    //Without: 195 125 45
    //With: 195 131 48
    //With plus: R= 206  G= 129  B= 44  
  
    bool correctColor = getBlue ? (currentG >= 148 && currentB >= 42)
                                  : (currentG >= 128);
  
    if(correctColor){
      blockValid = true;
      moveToEnd();
    }
    else {
      blockValid = false;
      if(attempts == 2){
        latestPosition = UNKNOWNPOS;  
      }
    }

    resetCurrentRGB();

    attempts++;
  }

  if(blockPushed && blockValid == false){
    blockPushed = false;
  }
  if(markerFinished && blockValid == false){
    markerFinished = false;
  }
}

void moveToEnd(){
  stopMotor = false;
  
  forkMotor.enable();
  forkMotor.rotate(46500);
  forkMotor.disable();

  latestPosition = END;
}

void moveToStock(int stockId){
  stopMotor = false;
  int rotations = 0;
  ForkPosition newPos;

  switch(stockId){
    case 1:
      rotations = 21100;
      newPos = STOCK1;
      break;
      
    case 2:
      rotations = 22100;
      newPos = STOCK2;
      break;
      
    default:
      return;
  }

  forkMotor.enable();
  
  if(stockId == 2){
    forkMotor.rotate(rotations);
  }
  forkMotor.rotate(rotations);
  
  forkMotor.disable();
  latestPosition = newPos;
}

void goToFirstCheck(){
  stopMotor = false;
  
  int rotations = 0;

  if(latestPosition == STOCK1){
    rotations = 21000;
  }
  
  if(latestPosition == STOCK2){
    rotations = 19000;
  }

  forkMotor.enable();

  if(latestPosition == STOCK1){
      forkMotor.rotate(rotations);
  }
  forkMotor.rotate(rotations);
  
  forkMotor.disable();
  latestPosition = COLORSENSOR1;
}

void goToSecondCheck(){
  stopMotor = false;
  
  int rotations = 0;

  forkMotor.enable();

  forkMotor.rotate(19000);
  
  forkMotor.disable();
  latestPosition = COLORSENSOR2;
}

void executeScan1(){
  digitalWrite(Color1S2, LOW);
  digitalWrite(Color1S3, LOW);

  frequency = pulseIn(Color1SensorOut, LOW);
  currentR = frequency;

  Serial.print("R= ");
  Serial.print(frequency);
  Serial.print("  ");

  delay(100);

  digitalWrite(Color1S2, HIGH);
  digitalWrite(Color1S3, HIGH);

  frequency = pulseIn(Color1SensorOut, LOW);
  currentG = frequency;

  Serial.print("G= ");
  Serial.print(frequency);
  Serial.print("  ");

  delay(100);

  digitalWrite(Color1S2, LOW);
  digitalWrite(Color1S3, HIGH);

  frequency = pulseIn(Color1SensorOut, LOW);
  currentB = frequency;

  Serial.print("B= ");
  Serial.print(frequency);
  Serial.print("  ");
  Serial.println();
}

void executeScan2(){
  digitalWrite(Color2S2, LOW);
  digitalWrite(Color2S3, LOW);

  frequency = pulseIn(Color2SensorOut, LOW);
  currentR = frequency;

  Serial.print("R= ");
  Serial.print(frequency);
  Serial.print("  ");

  delay(100);

  digitalWrite(Color2S2, HIGH);
  digitalWrite(Color2S3, HIGH);

  frequency = pulseIn(Color2SensorOut, LOW);
  currentG = frequency;

  Serial.print("G= ");
  Serial.print(frequency);
  Serial.print("  ");

  delay(100);

  digitalWrite(Color2S2, LOW);
  digitalWrite(Color2S3, HIGH);

  frequency = pulseIn(Color2SensorOut, LOW);
  currentB = frequency;

  Serial.print("B= ");
  Serial.print(frequency);
  Serial.print("  ");
  Serial.println();
}

void setEditTime(){
  editTime = 0;
  
  endTime = millis();

  editTime = (endTime - startTime) / 1000;

  startTime = 0;
  endTime = 0;
}

void onEditEnded(bool success){
  setEditTime();

  Serial3.println("-----------------------------");
  Serial3.println("Block edit finished.");
  Serial3.print("Success: "); Serial.println((String)success);
  Serial3.print("Edit took: "); Serial.print(editTime); Serial.println(" seconds.");
  Serial3.println("-----------------------------");

  sendLinesDrawn();
  
  editTime = 0;
}

void onInstructionsFinished(){
  Serial3.println("-----------------------------");
  Serial3.println("Done with current instructions!");
  Serial3.print("Required amount: "); Serial.println(amountToProduce);
  Serial3.print("Incorrect produced amount: "); Serial.println(incorrectProduced);
  Serial3.print("Correct produced amount: "); Serial.println(amountProduced);
  Serial3.print("Total: "); Serial.println(amountProduced + incorrectProduced);
  Serial3.println("-----------------------------");
}

void onNewInstructionsFound(bool blue, bool drawPlus){
  Serial3.println("-----------------------------");
  Serial3.println("New instructions found!");
  Serial3.print("Color: "); Serial.println(blue? "blue" : "green");
  Serial3.print("Drawing: "); Serial.println(drawPlus? "+" : "-");
  Serial3.print("Amount to produce: "); Serial.println(amountToProduce);
  Serial3.println("-----------------------------");
}

void onStockTooLow(int stockId){
  Serial3.println("-----------------------------");
  Serial3.print("ALERT: "); Serial.print(stockId == 1 ? "Green" : "Blue"); Serial.println(" is out of stock!");
  Serial3.println("-----------------------------");
}

void sendMotorTemp(){
  double temp = tempSensor.analog2temp();
  
  Serial3.println("-----------------------------");
  Serial3.println("Motor temperature update");
  Serial3.print("Motor temperature: "); Serial.println(temp);
  Serial3.println("-----------------------------");
}

void sendLinesDrawn(){
  Serial3.println("-----------------------------");
  Serial3.println("Lines drawn update");
  Serial3.print("Lines drawn: "); Serial.println(linesDrawn);
  Serial3.println("-----------------------------");
  
  linesDrawn = 0;
}

void resetProductionStats(){
  amountProduced = 0;
  amountToProduce = 0;
  incorrectProduced = 0;
}

void resetCurrentRGB(){
  currentR = 0;
  currentG = 0;
  currentB = 0;
}
