    #include <Servo.h>
    #include <IRremote.h>
    #include <EEPROM.h>

    // Pin definitions
    const int IR_RECEIVER_PIN = 2;
    const int SERVO_PIN = 9;

    // Operation mode configuration - Change this to switch modes
    const bool RECORDING_MODE_ENABLED = false; // Set to true for recording mode, false for normal mode

    // Servo configuration
    const int MIN_ANGLE = 92;
    const int MAX_ANGLE = 132;
    const int SERVO_STEP = 1; // Degrees per movement step

    // Speed and timing configuration
    const int MAXIMUM_SPEED = 80; // 1Hz (using 10 to represent 1.0Hz with decimal precision)
    const int MINIMUM_SPEED = 10;  // 0.2Hz (using 2 to represent 0.2Hz with decimal precision)
    const unsigned long SPEED_TO_INTERVAL_MULTIPLIER = 10000; // Base interval in ms (adjusted for new range)

    // EEPROM addresses for storing IR codes
    const int EEPROM_INCREASE_SPEED_ADDR = 0;
    const int EEPROM_DECREASE_SPEED_ADDR = 4;
    const int EEPROM_PAUSE_ADDR = 8;
    const int EEPROM_INIT_FLAG_ADDR = 12;
    const byte EEPROM_INIT_FLAG = 0xAA; // Flag to check if EEPROM has been initialized

    // Global variables
    Servo servo;
    IRrecv irrecv(IR_RECEIVER_PIN);
    decode_results results;

    // Operation modes
    enum OperationMode {
    RECORDING_MODE,
    NORMAL_MODE
    };

    OperationMode currentMode;

    // IR codes (loaded from EEPROM)
    uint32_t increaseSpeedCode = 0;
    uint32_t decreaseSpeedCode = 0;
    uint32_t pauseCode = 0;

    // Servo movement variables
    int currentAngle = MIN_ANGLE;
    int targetAngle = MAX_ANGLE;
    bool movingToMax = true;
    unsigned long lastServoUpdate = 0;
    unsigned long lastAngleChange = 0;

    // Speed control
    int currentSpeed = MAXIMUM_SPEED / 2; // Start at middle speed (0.6Hz)
    unsigned long INTERVAL = SPEED_TO_INTERVAL_MULTIPLIER / currentSpeed;

    // Pause state
    bool isPaused = false;
    bool movingToRest = false; // Flag to indicate slow movement to rest position
    unsigned long lastRestMovement = 0;
    const int REST_MOVEMENT_DELAY = 50; // ms between each degree when moving to rest

    void setup() {
    Serial.begin(9600);
    Serial.println("Baby Rocker Controller Starting...");
    
    // Initialize servo
    servo.attach(SERVO_PIN);
    servo.write(MIN_ANGLE);
    currentAngle = MIN_ANGLE;
    
    // Initialize IR receiver
    irrecv.enableIRIn();
    
    // Determine operation mode based on constant
    if (RECORDING_MODE_ENABLED) {
        currentMode = RECORDING_MODE;
        Serial.println("RECORDING MODE - Ready to record IR codes");
        recordIRCodes();
    } else {
        currentMode = NORMAL_MODE;
        Serial.println("NORMAL MODE - Loading IR codes from EEPROM");
        loadIRCodesFromEEPROM();
        Serial.println("Ready for normal operation");
    }
    
    lastServoUpdate = millis();
    lastAngleChange = millis();
    }

    void loop() {
    if (currentMode == NORMAL_MODE) {
        // Check for IR input
        checkIRInput();
        
        // Update servo position if not paused
        if (!isPaused) {
        updateServoPosition();
        }
    }
    // Recording mode is handled in setup() and doesn't loop
    }

    void recordIRCodes() {
    Serial.println("Recording IR codes...");
    
    // Record INCREASE SPEED code
    Serial.println("Point remote and press INCREASE SPEED button...");
    uint32_t code1 = waitForIRCode();
    increaseSpeedCode = code1;
    EEPROM.put(EEPROM_INCREASE_SPEED_ADDR, increaseSpeedCode);
    Serial.print("INCREASE SPEED code recorded: 0x");
    Serial.println(increaseSpeedCode, HEX);
    
    delay(1000); // Brief pause between recordings
    
    // Record DECREASE SPEED code
    Serial.println("Point remote and press DECREASE SPEED button...");
    uint32_t code2 = waitForIRCode();
    decreaseSpeedCode = code2;
    EEPROM.put(EEPROM_DECREASE_SPEED_ADDR, decreaseSpeedCode);
    Serial.print("DECREASE SPEED code recorded: 0x");
    Serial.println(decreaseSpeedCode, HEX);
    
    delay(1000); // Brief pause between recordings
    
    // Record PAUSE code
    Serial.println("Point remote and press PAUSE button...");
    uint32_t code3 = waitForIRCode();
    pauseCode = code3;
    EEPROM.put(EEPROM_PAUSE_ADDR, pauseCode);
    Serial.print("PAUSE code recorded: 0x");
    Serial.println(pauseCode, HEX);
    
    // Set initialization flag
    EEPROM.write(EEPROM_INIT_FLAG_ADDR, EEPROM_INIT_FLAG);
    
    Serial.println("All IR codes recorded successfully!");
    Serial.println("Switch to NORMAL mode and reset to use the rocker.");
    
    // Stay in recording mode (infinite loop)
    while (true) {
        delay(1000);
    }
    }

    uint32_t waitForIRCode() {
    while (true) {
        if (irrecv.decode(&results)) {
        uint32_t code = results.value;
        irrecv.resume(); // Receive the next value
        
        // Ignore repeat codes and invalid codes
        if (code != 0xFFFFFFFF && code != 0) {
            Serial.print("Received code: 0x");
            Serial.println(code, HEX);
            return code;
        }
        }
        delay(100);
    }
    }

    void loadIRCodesFromEEPROM() {
    // Check if EEPROM has been initialized
    if (EEPROM.read(EEPROM_INIT_FLAG_ADDR) != EEPROM_INIT_FLAG) {
        Serial.println("ERROR: EEPROM not initialized. Please run in RECORDING mode first.");
        while (true) {
        delay(1000); // Halt execution
        }
    }
    
    // Load IR codes from EEPROM
    EEPROM.get(EEPROM_INCREASE_SPEED_ADDR, increaseSpeedCode);
    EEPROM.get(EEPROM_DECREASE_SPEED_ADDR, decreaseSpeedCode);
    EEPROM.get(EEPROM_PAUSE_ADDR, pauseCode);
    
    Serial.print("Loaded INCREASE SPEED code: 0x");
    Serial.println(increaseSpeedCode, HEX);
    Serial.print("Loaded DECREASE SPEED code: 0x");
    Serial.println(decreaseSpeedCode, HEX);
    Serial.print("Loaded PAUSE code: 0x");
    Serial.println(pauseCode, HEX);
    }

    void checkIRInput() {
    if (irrecv.decode(&results)) {
        uint32_t receivedCode = results.value;
        irrecv.resume(); // Receive the next value
        
        // Ignore repeat codes
        if (receivedCode != 0xFFFFFFFF && receivedCode != 0) {
        Serial.print("IR code received: 0x");
        Serial.println(receivedCode, HEX);
        
        if (receivedCode == increaseSpeedCode) {
            increaseSpeed();
        } else if (receivedCode == decreaseSpeedCode) {
            decreaseSpeed();
        } else if (receivedCode == pauseCode) {
            togglePause();
        } else {
            Serial.println("Unknown IR code received");
        }
        }
    }
    }

    void increaseSpeed() {
    if (currentSpeed < MAXIMUM_SPEED) {
        currentSpeed = currentSpeed + 3;
        updateInterval();
        Serial.print("Speed increased to: ");
        Serial.print(currentSpeed / 10.0, 1);
        Serial.println("Hz");
    } else {
        Serial.println("Already at maximum speed");
    }
    }

    void decreaseSpeed() {
    if (currentSpeed > MINIMUM_SPEED) {
        currentSpeed = currentSpeed - 3;
        updateInterval();
        Serial.print("Speed decreased to: ");
        Serial.print(currentSpeed / 10.0, 1);
        Serial.println("Hz");
    } else {
        Serial.println("Already at minimum speed");
    }
    }

    void updateInterval() {
    // INTERVAL is now the time for one complete cycle (min->max->min) in milliseconds
    // currentSpeed represents frequency * 10 (so 40 = 4.0Hz)
    INTERVAL = 10000 / currentSpeed; // Complete cycle time in ms
    Serial.print("New cycle time: ");
    Serial.print(INTERVAL);
    Serial.println("ms");
    }

    void togglePause() {
    isPaused = !isPaused;
    if (isPaused) {
        Serial.println("PAUSED - Moving to minimum angle and detaching servo");
        servo.write(MIN_ANGLE);
        delay(500); // Give servo time to reach position before detaching
        servo.detach();
        currentAngle = MIN_ANGLE;
        targetAngle = MAX_ANGLE;
        movingToMax = true;
        movingToRest = true;
        lastRestMovement = millis();
    } else {
        Serial.println("RESUMED - Reattaching servo and continuing normal operation");
        servo.attach(SERVO_PIN);
        servo.write(MIN_ANGLE); // Ensure servo starts at minimum angle
        currentAngle = MIN_ANGLE;
        lastServoUpdate = millis();
        lastAngleChange = millis();
    }
    }

    void updateServoPosition() {
    unsigned long currentTime = millis();
    
    // Check if it's time to move the servo
    if (currentTime - lastServoUpdate >= 20) { // Update every 20ms for smooth movement
        lastServoUpdate = currentTime;
        
        // Calculate time between each step
        // One complete cycle = min->max->min = 2 * (MAX_ANGLE - MIN_ANGLE) steps
        unsigned long totalSteps = 2 * (MAX_ANGLE - MIN_ANGLE) / SERVO_STEP;
        unsigned long stepInterval = INTERVAL / totalSteps;
        
        // Ensure minimum step interval to prevent too fast movement
        if (stepInterval < 20) stepInterval = 20;
        
        if (currentTime - lastAngleChange >= stepInterval) {
        lastAngleChange = currentTime;
        
        // Move servo one step
        if (movingToMax) {
            currentAngle += SERVO_STEP;
            if (currentAngle >= MAX_ANGLE) {
            currentAngle = MAX_ANGLE;
            movingToMax = false;
            targetAngle = MIN_ANGLE;
            Serial.println("Reached maximum angle, reversing direction");
            }
        } else {
            currentAngle -= SERVO_STEP;
            if (currentAngle <= MIN_ANGLE) {
            currentAngle = MIN_ANGLE;
            movingToMax = true;
            targetAngle = MAX_ANGLE;
            Serial.println("Reached minimum angle, reversing direction");
            }
        }
        
        servo.write(currentAngle);
        }
    }

    // Check if it's time to move to rest position
    if (movingToRest && (currentTime - lastRestMovement >= REST_MOVEMENT_DELAY)) {
        lastRestMovement = currentTime;
        
        // Move to rest position
        if (currentAngle < MIN_ANGLE) {
        currentAngle += SERVO_STEP;
        if (currentAngle >= MIN_ANGLE) {
            currentAngle = MIN_ANGLE;
            movingToRest = false;
            Serial.println("Reached rest position");
        }
        } else {
        currentAngle -= SERVO_STEP;
        if (currentAngle <= MIN_ANGLE) {
            currentAngle = MIN_ANGLE;
            movingToRest = false;
            Serial.println("Reached rest position");
        }
        }
        
        servo.write(currentAngle);
    }
    } 