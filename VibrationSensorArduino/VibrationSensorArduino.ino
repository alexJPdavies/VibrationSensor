/*Libaries Included*/
#include <Wire.h>            // Speaking over i2C with adafruit sensor
#include <Adafruit_Sensor.h> // Adafruit libary required with hardware
#include <Adafruit_BNO055.h> // Adafruit BNO055 accelorometer
#include <SD.h>              // Enables reading from and writing
#include <SPI.h>             // Enables synchronous serial data protocol communication
#include "BluetoothSerial.h" // Enables Bluetooth
#include "arduinoFFT.h"      // Fast Fourier Transform Libaryhttps://github.com/kosme/arduinoFFT
#include <ArduinoJson.h>      // https://techtutorialsx.com/2017/04/27/esp32-creating-json-message/

/*define constant required for FFT printVector function*/
#define SCL_INDEX 0x00
#define SCL_TIME 0x01
#define SCL_FREQUENCY 0x02
#define SCL_PLOT 0x03

/*Create object*/
Adafruit_BNO055 myIMU = Adafruit_BNO055(); // Define Accelorometer serial object called 'myIMU'
File myFile; // File object called 'myFile'
BluetoothSerial SerialBT; //Bluetooth serial
arduinoFFT FFT = arduinoFFT(); // Create FFT object called 'FFT'

/*Create values required for FFT function*/
const uint16_t samples = 256; //This value MUST ALWAYS be a power of 2
const double samplingFrequency = 130; // Must sample twice the rate of the fundermental frequency (Hz)
const double sampling_period = 7692.3; // 1/samplingFrequency (microsecnds)
int microseconds ; // Used in while loop for constant time stamp

/*
These are the input and output vectors
Input vectors receive computed results from FFT
*/
double vReal[samples];
double vImag[samples];

void setup() {
  /*Start serial comunication*/
  Serial.begin(115200);
  /*Initialising BNO055*/
  if (myIMU.begin())
  {
    Serial.println("BNO055 ready to use."); //if statement = true, write 'BNO055 ready to use.' to serial monitor
  } else
  {
    Serial.println("BNO055 initialization failed");// Otherwise write 'BNO055 initialization failed' to serial monitor
    return;
  }
/* Initialising SD Card */
  if (SD.begin())
   {
    Serial.println("SD card is ready to use.");//if statement = true, write 'SD card is ready to use.' to serial monitor
  } else
  {
    Serial.println("SD card initialisation failed");// Otherwise write 'SD card initialisation failed' to serial monitor
    return;
  }
  /* Create .txt file on SD card & write header */
  myFile = SD.open("/BNO055.txt", FILE_WRITE); //Create .txt file named   
  if (myFile) {
  myFile.println("Vibration Sensor Data"); // Print header to BNO055.txt file
  myFile.close();// Close .txt file
  Serial.println("Printed Header to SD card"); // Print text to serial monitor to confirm header has been printed                                       
  }
  else {
    Serial.println("error writing header to MPU6050 during setup.txt");// If 'myFile' dosen't exist print message to serial monitor
  }
  SerialBT.begin("VibrationSensor"); //Initialise Bluetooth and create device name
  Serial.println("The device started, now you can pair it with bluetooth!");

}

void loop() {
  /* Initialise Bluetooth write*/
  SerialBT.write(Serial.read());
  Serial.write(SerialBT.read());

  

  /*Retrive Acceleration vector and insert into array*/
  // Starting for loop
  for (int i=0; i<samples; i++)
  {
    microseconds = micros(); // passing the microsecond value at the beginning of the loop
    // getting values from accelorometer and storing vReal array for every increment of i
    imu::Vector<3> acc = myIMU.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
    vReal[i] = (acc.y());// Build data with positive and negative values
    vImag[i] = 0.0; //Imaginary part must be zeroed in case of looping to avoid wrong calculations and overflows
    while ((micros() - microseconds) <sampling_period); // control constant time-stamp = sampling period
  }
  
 /* Print results of the simulated sampling according to time OR Frequency */
  myFile = SD.open("/BNO055.txt", FILE_APPEND); //Open file BNO055.txt
  if (myFile) { // If file is open carry out block of code
  Serial.println ("Data:");
  myFile.println("Data:");
  
  PrintVector(vReal, samples, SCL_TIME); //VReal, samples & SCL_Time passed to 'PrintVector' function. SCL_TIme is used in case statement.
  FFT.Windowing(vReal, samples, FFT_WIN_TYP_HAMMING, FFT_FORWARD);  /* Weigh data */
  FFT.Compute(vReal, vImag, samples, FFT_FORWARD); /* Compute FFT */
  FFT.ComplexToMagnitude(vReal, vImag, samples); /* Compute magnitudes */
  Serial.println("Computed magnitudes:");
  myFile.println("Computed magnitudes:");
  
  PrintVector(vReal, (samples >> 1), SCL_FREQUENCY);  // Pribt vReal values in frequency domain 
  double x; // Creating variable 'x'
  double v; //Creating variable 'v'
  FFT.MajorPeak(vReal, samples, samplingFrequency, &x, &v); // Calculating peaks for 'x' & 'v'
  Serial.print(x, 4); // Printing 'x' value to 4 decimal places
  //SerialBT.print(x, 4);
  myFile.print(x, 4);
  Serial.print(", ");
  //SerialBT.print(", ");
  myFile.print (", ");
  Serial.println(v, 4);// Printing 'v' value to 4 decimal places
  //SerialBT.println(v, 4);
  myFile.println(v, 4);

  myFile.println();
  myFile.close(); // Closing file, so it can be re opened on the next loop and data appended
  Serial.println("Vibration Sensor data printed to SD card");
  }
  else {
    Serial.println("error writing header to BNO055.txt"); // If file didnt open, text written in serial monitor
 }
  delay (10); /* continuous loop on delay*/
  //while(1); /* loop run once */
}
  /*PrintVector function outside the void loop function*/
  void PrintVector(double *vData, uint16_t bufferSize, uint8_t scaleType)// 'vReal' variable name is changed to 'vData with a data type of double
 {
  /*Creating Json document, write directly to it*/
  DynamicJsonDocument doc(1024);
    
  /* Calculate abscissa value */
    switch (scaleType) //'scaleType' is passed within 'PrintVector()', from the 'void loop ()' function.
    {
      case SCL_INDEX:
        doc["DataType"] = "Index";        
  break;
      case SCL_TIME:
        doc["DataType"] = "Time";       
  break;
      case SCL_FREQUENCY:
        doc["DataType"] = "Frequency";
  break;
    }

  for (uint16_t i = 0; i < bufferSize; i++)
  {
    double abscissa;
    /* Calculate abscissa value */
    switch (scaleType) //'scaleType' is passed within 'PrintVector()', from the 'void loop ()' function.
    {
      case SCL_INDEX:
        abscissa = (i * 1.0);
  break;
      case SCL_TIME:  
        abscissa = ((i * 1.0) / samplingFrequency); // Used to print time domain values
  break;
      case SCL_FREQUENCY:  
        abscissa = ((i * 1.0 * samplingFrequency) / samples); // Used to print frequency domian values
  break;
    }

//     Serial.print(abscissa, 4); // An optional second parameter is passed, specifiying the number of decimal places used '6'
//     SerialBT.print(abscissa, 4);
     myFile.print(abscissa, 4);
     /*Internal array*/
     doc["values"][0] = abscissa;
     
     if(scaleType==SCL_FREQUENCY)// Hz are added if scaleType = SCL_FREQUENCY
     {
//        Serial.print("Hz");
//        SerialBT.print("Hz");
        myFile.print ("Hz");
    }
//    Serial.print(","); // vData is printed to serial, bluetooth and myFile.
//    SerialBT.print(",");
    myFile.print (",");
//    Serial.println(vData[i], 4);
//    SerialBT.println(vData[i], 4);
    myFile.println (vData[i], 4);
    doc["values"][1] = vData [i];
    serializeJson(doc,Serial);
    serializeJson(doc,SerialBT);
    SerialBT.println("\r\n");
    SerialBT.flush();
    Serial.println("\r\n");  
  }
Serial.println();
myFile.println();
} // 'void PrintVector' end
