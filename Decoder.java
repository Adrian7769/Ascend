import java.util.Scanner;
public class Decoder {
    public static void main(String[] args) {
        Scanner scanner = new Scanner(System.in);
        boolean flagexit = false;
        while (!flagexit) {
            // Prompt the user for the hexadecimal string
            System.out.println("Enter the hexadecimal string (or enter 0 to quit):");
            String hexInput = scanner.nextLine(); // Read the input as a string
            // Check if the user wants to exit
            if (hexInput.equals("0")) {
                System.out.println("Exiting program. Goodbye!");
                flagexit = true; // Exit the loop
                continue; // Skip further processing
            }
            // Validate that the input contains exactly 100 characters (50 bytes)
            if (hexInput.length() > 100) {
                System.out.println("Error: Input must contain exactly 100 hexadecimal characters (50 bytes). Please try again.");
                continue; // Restart the loop
            }
            System.out.println("\nHexadecimal Input: " + hexInput);
            System.out.println("\nDecimal Output: ");
            convertHexToDecimal(hexInput);
        }
        scanner.close(); // Close the scanner
    }
    public static void convertHexToDecimal(String hexString) {
        // Array to store decimal values
        int[] decimalValues = new int[hexString.length() / 2];
        // Convert hex to decimal
        for (int i = 0; i < hexString.length(); i += 2) {
            String hexPair = hexString.substring(i, i + 2);
            int decimalValue = Integer.parseInt(hexPair, 16);
            decimalValues[i / 2] = decimalValue;
            System.out.print(decimalValue + " ");
        }
        System.out.println(); // Formatting
        // Convert the first two decimal numbers to ASCII
        char ascii1 = (char) decimalValues[0];
        char ascii2 = (char) decimalValues[1];
        System.out.println("\nASCII Representation: " + ascii1 + ascii2);
        // Calculate the RockBLOCK serial number (indices 2, 3, and 4)
        int rockBlockSerial = (decimalValues[2] << 16) + (decimalValues[3] << 8) + decimalValues[4];
        System.out.println("RockBLOCK Serial Number: " + rockBlockSerial);
        // Process UTC time (indices 5, 6, 7)
                // Convert UTC to Arizona time
        int arizonaHours24 = (decimalValues[5] - 7 + 24) % 24; // Adjust for UTC-7
        // Convert 24-hour time to 12-hour time
        int arizonaHours12;
        if (arizonaHours24 == 0) {
            arizonaHours12 = 12; // Midnight
        } else if (arizonaHours24 == 12) {
            arizonaHours12 = 12; // Noon
        } else if (arizonaHours24 > 12) {
            arizonaHours12 = arizonaHours24 - 12; // Convert PM hours
        } else {
            arizonaHours12 = arizonaHours24; // AM hours remain the same
        }
        // Determine AM/PM
        String amPm;
        if (arizonaHours24 < 12) {
            amPm = "AM";
        } else {
            amPm = "PM";
        }
        // Print Arizona time in 12-hour format
        System.out.println("ARIZONA Time: " + arizonaHours12 + ":" + decimalValues[6] + ":" + decimalValues[7] + " " + amPm);
        System.out.println("UTC Time: " + decimalValues[5] + " hours, " + decimalValues[6] + " minutes, " + decimalValues[7] + " seconds");
        // Process GPS data (indices 8 to 15)
        System.out.println("Latitude: " + decimalValues[8] + "° " + decimalValues[9] + "' " + decimalValues[10] + "\" " + 
                           (decimalValues[11] == 0 ? "N" : "S"));
        System.out.println("Longitude: " + decimalValues[12] + "° " + decimalValues[13] + "' " + decimalValues[14] + "\" " + 
                           (decimalValues[15] == 0 ? "E" : "W"));
        // Calculate latitude in decimal
        double latitudeDecimal = decimalValues[8] + (decimalValues[9] / 60.0) + (decimalValues[10] / 3600.0);
        if (decimalValues[11] != 0) { // Check if South
            latitudeDecimal = -latitudeDecimal;
        }
        // Calculate longitude in decimal
        double longitudeDecimal = decimalValues[12] + (decimalValues[13] / 60.0) + (decimalValues[14] / 3600.0);
        if (decimalValues[15] != 0) { // Check if West
            longitudeDecimal = -longitudeDecimal;
        }
        // Print latitude in decimal & longitude in decimal
        System.out.println("Latitude in Decimal: " + latitudeDecimal);
        System.out.println("Longitude in Decimal: " + longitudeDecimal);
        // Process Altitude (indices 16 to 19)
        int altitude = (decimalValues[16] << 24) | (decimalValues[17] << 16) | (decimalValues[18] << 8) | decimalValues[19];
        System.out.println("Altitude: " + altitude + (decimalValues[20] == 0 ? " meters" : " feet"));
        /* -------------- OLD CODE --------------
        // Process Analog Data (indices 21 to 36)
        System.out.println("Analog Data:");
        for (int i = 21; i <= 34; i += 2) {
            int msb = decimalValues[i];
            int lsb = decimalValues[i + 1];
            int rawValue = (msb * 256) + lsb;
            float voltage = rawValue * 0.004888f; // Conversion to voltage
            System.out.println("Port " + ((i - 21) / 2) + ": " + voltage + " V");
        }
        // Process Analog Data (indices 21 to 36)
        System.out.println("Analog Data:");
        for (int i = 21; i <= 34; i += 2) {
            int msb = decimalValues[i];
            int lsb = decimalValues[i + 1];
            int rawValue = (msb * 256) + lsb;
            float voltage = rawValue * 0.004888f; // Conversion to voltage
            int port = (i - 21) / 2;
            float measurement = 0;
            String unit = "";
            String sensorName = "";
            switch (port) {
                case 0: // Internal Temperature
                    sensorName = "Internal Temperature";
                    measurement = (voltage - 0.489f) / 0.0096f;
                    unit = "°C";
                    break;
                case 1: // Pressure
                    sensorName = "Pressure Sensor";
                    measurement = (voltage - 0.580f) / 0.267f;
                    unit = "PSI";
                    break;
                case 2: // UV Light Sensor
                    sensorName = "UV Light Sensor";
                    measurement = voltage / 0.1f; // Convert voltage to UV Index
                    unit = "UV Index";
                    break;
                case 3: // N/A (Unused Port)
                    sensorName = "Unused Port";
                    measurement = voltage;
                    unit = "V";
                    break;
                case 4: // External Temperature
                    sensorName = "External Temperature";
                    measurement = (voltage - 0.495f) / 0.0095f;
                    unit = "°C";
                    break;
                case 5: // Y-axis Acceleration
                    sensorName = "Y-axis Accelerometer";
                    measurement = (voltage - 1.612f) / 0.222f;
                    unit = "G";
                    break;
                case 6: // X-axis Acceleration
                    sensorName = "X-axis Accelerometer";
                    measurement = (voltage - 1.623f) / 0.21f;
                    unit = "G";
                    break;
                default: // Other ports (if applicable)
                    sensorName = "Unknown Sensor";
                    measurement = voltage;
                    unit = "V";
                    break;
            }
            System.out.printf("Port %d, %s, Voltage: %.3f V, Measurement: %.3f %s%n", 
                              port, sensorName, voltage, measurement, unit);
        }
        */
        // Process Analog Data (indices 21 to 36)
        System.out.println("Analog Data:");
        for (int i = 21; i <= 34; i += 2) {
            int msb = decimalValues[i];
            int lsb = decimalValues[i + 1];
            int rawValue = (msb * 256) + lsb;
            float voltage = rawValue * 0.004888f; // Conversion to voltage
            String sensorName;
            String measurement;
            switch ((i - 21) / 2) {
                case 0: // Internal Temperature
                    sensorName = "Internal Temperature";
                    float tempC = (voltage - 0.489f) / 0.0096f;
                    float tempF = (tempC * 1.8f) + 32;
                    measurement = String.format("%.2f °C / %.2f °F", tempC, tempF);
                    break;
                case 1: // Pressure
                    sensorName = "Pressure";
                    float pressure = (voltage - 0.580f) / 0.267f;
                    measurement = String.format("%.2f PSI", pressure);
                    break;
                case 2: // UV Light Sensor
                    sensorName = "UV Light Sensor";
                    float uvIndex = voltage / 0.1f;
                    measurement = String.format("%.2f UV Index", uvIndex);
                    break;
                case 3: // N/A
                    sensorName = "N/A";
                    measurement = String.format("%.2f V", voltage);
                    break;
                case 4: // External Temperature
                    sensorName = "External Temperature";
                    float extTempC = (voltage - 0.495f) / 0.0095f;
                    float extTempF = (extTempC * 1.8f) + 32;
                    measurement = String.format("%.2f °C / %.2f °F", extTempC, extTempF);
                    break;
                case 5: // Acceleration (Y-axis)
                    sensorName = "Acceleration Y-axis";
                    float accelY = (voltage - 1.612f) / 0.222f;
                    measurement = String.format("%.4f G", accelY);
                    break;
                case 6: // Acceleration (X-axis)
                    sensorName = "Acceleration X-axis";
                    float accelX = (voltage - 1.623f) / 0.21f;
                    measurement = String.format("%.4f G", accelX);
                    break;
                default:
                    sensorName = "Unknown";
                    measurement = String.format("%.4f V", voltage);
            }
            System.out.println(String.format("Port %d, %s, %.3f V, %s", (i - 21) / 2, sensorName, voltage, measurement));
        }
        // Special handling for battery data (indices 35 and 36)
        int batteryMSB = decimalValues[35];
        int batteryLSB = decimalValues[36];
        int batteryRaw = (batteryMSB * 256) + batteryLSB;
        float voltageConstant = 0.004888f;
        float batteryVoltage = (batteryRaw * voltageConstant) * 2; // Multiply by 2 for battery
        System.out.println("Battery Voltage: " + String.format("%.3f", batteryVoltage) + " V");
        // Success Code (indices 48 and 49)
        System.out.println();
        int statusMSB = decimalValues[48];
        int statusLSB = decimalValues[49];
        int statusCode = (statusMSB * 256) + statusLSB;
        System.out.println("Modem Return Code: " + statusCode);
            switch (statusCode) {
                default:
                    System.out.println("Status: Unknown or unhandled status code.");
                    break;
                // -- MO Message Return Codes (0-65) --
                case 0:
                    System.out.println("Status: MO message transferred successfully.");
                    break;
                case 1:
                    System.out.println("Status: MO message transferred successfully, but MT message in the queue was too big.");
                    break;
                case 2:
                    System.out.println("Status: MO message transferred successfully, but Location Update was not accepted.");
                    break;
                case 3:
                case 4:
                    System.out.println("Status: Reserved – indicates MO session success.");
                    break;
                case 5:
                case 6:
                case 7:
                case 8:
                    System.out.println("Status: Reserved – indicates MO session failure.");
                    break;
                case 10:
                    System.out.println("Status: GSS reported that the call did not complete in the allowed time.");
                    break;
                case 11:
                    System.out.println("Status: MO message queue at the GSS is full.");
                    break;
                case 12:
                    System.out.println("Status: MO message has too many segments.");
                    break;
                case 13:
                    System.out.println("Status: GSS reported that the session did not complete.");
                    break;
                case 14:
                    System.out.println("Status: Invalid segment size.");
                    break;
                case 15:
                    System.out.println("Status: Access is denied.");
                    break;
                case 16:
                    System.out.println("Status: ISU has been locked and may not make SBD calls (see +CULK command).");
                    break;
                case 17:
                    System.out.println("Status: Gateway not responding (local session timeout).");
                    break;
                case 18:
                    System.out.println("Status: Connection lost (RF drop).");
                    break;
                case 19:
                    System.out.println("Status: Link failure (protocol error caused termination of the call).");
                    break;
                // Reserved range 20-31
                case 20: case 21: case 22: case 23: case 24:
                case 25: case 26: case 27: case 28: case 29:
                case 30: case 31:
                    System.out.println("Status: Reserved return code indicating failure.");
                    break;
                case 32:
                    System.out.println("Status: No network service, unable to initiate call.");
                    break;
                case 33:
                    System.out.println("Status: Antenna fault, unable to initiate call.");
                    break;
                case 34:
                    System.out.println("Status: Radio is disabled, unable to initiate call (see *Rn command).");
                    break;
                case 35:
                    System.out.println("Status: ISU is busy, unable to initiate call.");
                    break;
                case 36:
                    System.out.println("Status: Try later, must wait 3 minutes since last registration.");
                    break;
                case 37:
                    System.out.println("Status: SBD service is temporarily disabled.");
                    break;
                case 38:
                    System.out.println("Status: Try later, traffic management period (see +SBDLOE command).");
                    break;
                // Reserved range 39-63
                case 39: case 40: case 41: case 42: case 43:
                case 44: case 45: case 46: case 47: case 48:
                case 49: case 50: case 51: case 52: case 53:
                case 54: case 55: case 56: case 57: case 58:
                case 59: case 60: case 61: case 62: case 63:
                    System.out.println("Status: Reserved return code indicating failure.");
                    break;
                case 64:
                    System.out.println("Status: Band violation (attempt to transmit outside permitted frequency band).");
                    break;
                case 65:
                    System.out.println("Status: PLL lock failure hardware error during attempted transmit.");
                    break;
                // -- Failure Return Codes (100-299) -- 
                case 100:
                System.out.println("Status: FailureAfterSBDIX_UInt");
                    break;
                case 101:
                    System.out.println("Status: ModemStatus_TimedOutUInt");
                    break;
                case 104:
                    System.out.println("Status: UnexpectedMO_StatusValueUInt");
                    break;
                case 105:
                    System.out.println("Status: UnexpectedMOMSN_ValueUInt");
                    break;
                case 106:
                    System.out.println("Status: UnexpectedMT_StatusValueUInt");
                    break;
                case 107:
                    System.out.println("Status: UnexpectedMTMSN_StatusValueUInt");
                    break;
                case 108:
                    System.out.println("Status: UnexpectedMT_SBD_MessageLengthUInt");
                    break;
                case 109:
                    System.out.println("Status: UnexpectedMT_SBD_MessageQueuedValueUInt");
                    break;
                case 112:
                    System.out.println("Status: ModemFailureAfterSBDIX_UInt");
                    break;
                case 113:
                    System.out.println("Status: UnexpectedResponseToSBDIXUInt");
                    break;
                case 114:
                    System.out.println("Status: TimeOutAfterSBDIX_UInt");
                    break;
                case 116:
                    System.out.println("Status: TimeOutAfterSendingMessageSizeUInt");
                    break;
                case 118:
                    System.out.println("Status: MO_BufferClearedErrorResponseUInt");
                    break;
                case 119:
                    System.out.println("Status: MO_BufferClearedTimeOutUInt");
                    break;
                case 120:
                    System.out.println("Status: InvalidCommand");
                    break;
                case 200:
                    System.out.println("Status: MO_BufferClearedUnexpectedResponseUInt");
                    break;
                case 202:
                    System.out.println("Status: MT_BufferClearedErrorResponseUInt");
                    break;
                case 203:
                    System.out.println("Status: MT_BufferClearedTimeOutUInt");
                    break;
                case 204:
                    System.out.println("Status: MT_BufferClearedUnexpectedResponseUInt");
                    break;
                case 206:
                    System.out.println("Status: DisableFlowControlRequestTimedOutUInt");
                    break;
                case 207:
                    System.out.println("Status: DisableFlowControlRequestUnexpectedResponseUInt");
                    break;
                case 208:
                    System.out.println("Status: DisableSBD_RingSetupFailedDueToTimeOutUInt");
                    break;
                case 209:
                    System.out.println("Status: DisableSBD_RingSetupFailedDueToUnexpectedResponseUInt");
                    break;
                case 231:
                    System.out.println("Status: RingIndicationErrononiouslyEnabledUInt");
                    break;
                case 232:
                    System.out.println("Status: TimeOutAfterGetResponseFromVerifyDisableMT_AlertUInt");
                    break;
                case 233:
                    System.out.println("Status: UnexpectedResponseToSBDMTAUInt");
                    break;
                case 234:
                    System.out.println("Status: UnexpectedResponseAfterSendingMessageSizeUInt");
                    break;
                case 236:
                    System.out.println("Status: SetupFailedDueToTimeOutUInt");
                    break;
                case 237:
                    System.out.println("Status: SetupFailedDueToUnexpectedResponseUInt");
                    break;
                case 238:
                    System.out.println("Status: NetworkStatusUnexpectedResponseUInt");
                    break;
                case 239:
                    System.out.println("Status: TimeOutNetworkStatusUInt");
                    break;
                case 240:
                    System.out.println("Status: NetworkNotAvailableUInt");
                    break;
                case 242:
                    System.out.println("Status: MT_MessageUnexpectedResponseUInt");
                    break;
                case 243:
                    System.out.println("Status: MT_MessageTimeOutUInt");
                    break;
                case 244:
                    System.out.println("Status: MT_MessageFailedCheckSumUInt");
                    break;
                case 245:
                    System.out.println("Status: MT_MessageTooLongUInt");
                    break;
                case 249:
                    System.out.println("Status: ModemSetupFailedDueToTimeOutUInt");
                    break;
                case 250:
                    System.out.println("Status: NoModemConnectedUInt");
                    break;
                case 251:
                    System.out.println("Status: UnexpectedModemConnectedUInt");
                    break;
                case 255:
                    System.out.println("Status: DuplicateTransmitOfDataAttemptedUInt");
                    break;
                case 256:
                    System.out.println("Status: YouAreAskingToTransmitTooSoonSoTryLaterUInt");
                    break;
                case 257:
                    System.out.println("Status: FlowControlSetupFailedDueToTimeOutUInt");
                    break;
                case 258:
                    System.out.println("Status: FlowControlSetupFailedDueToUnexpectedResponseUInt");
                    break;
                case 259:
                    System.out.println("Status: StoreConfigurationFailedDueToTimeOutUInt");
                    break;
                case 260:
                    System.out.println("Status: StoreConfigurationFailedDueToUnexpectedResponseUInt");
                    break;
                case 261:
                    System.out.println("Status: SelectProfileFailedDueToTimeOutUInt");
                    break;
                case 262:
                    System.out.println("Status: SelectProfileFailedDueToUnexpectedResponseUInt");
                    break;
                case 263:
                    System.out.println("Status: RingIndicationUnexpectedResponseUInt");
                    break;
                case 264:
                    System.out.println("Status: OK_SearchTimedOutUInt");
                    break;
                case 265:
                    System.out.println("Status: OK_UnexpectedResponseUInt");
                    break;
                case 269:
                    System.out.println("Status: SignalStrengthTooLowUInt");
                    break;
                case 272:
                    System.out.println("Status: TransmitSuccessfulButReceiveFailedUInt");
                    break;
                case 273:
                    System.out.println("Status: UnexpectedFDR_CommandUInt");
                    break;
                case 274:
                    System.out.println("Status: MPM_Busy_TransmitCommandRejectedUInt");
                    break;
                case 275:
                    System.out.println("Status: PingToMPM_TimedOutUInt");
                    break;
                case 276:
                    System.out.println("Status: PingToMPM_SuccessButToModemFailedUInt");
                    break;
                case 278:
                    System.out.println("Status: InitialSetupModemStatusValueUInt");
                    break;
                case 279:
                    System.out.println("Status: TimeOutWaitingForgetReceivedDataUInt");
                    break;
                case 280:
                    System.out.println("Status: NoPingMPM_BusyUInt");
                    break;
                case 281:
                    System.out.println("Status: ModemFailedAtSetupUInt");
                    break;
                case 282:
                    System.out.println("Status: UnexpectedResponseFromModemDuringSetupUInt");
                    break;
                case 283:
                    System.out.println("Status: ModemSetupFailedBecauseMPM_BusyUInt");
                    break;
                case 284:
                    System.out.println("Status: ModemFailedAtSetupTimeOutUInt");
                    break;
                case 285:
                    System.out.println("Status: MPM_BusyWhenFDR_AskedForSetupUInt");
                    break;
                case 286:
                    System.out.println("Status: MPM_DidNotRespondToRequestForDataUInt");
                    break;
                case 287:
                    System.out.println("Status: SoftwareError1UInt");
                    break;
                case 288:
                    System.out.println("Status: MPM_BusyUInt");
                    break;
                case 289:
                    System.out.println("Status: AskedForPingResultTooSoon_DoPingAgainUInt");
                    break;
                case 290:
                    System.out.println("Status: PingToMPM_DidNotRespondUInt");
                    break;
                case 291:
                    System.out.println("Status: WrongModemConnectedCheckSerialNumberUInt");
                    break;
                case 292:
                    System.out.println("Status: RequestedTransmitTooSoonUInt");
                    break;
                case 293:
                    System.out.println("Status: NoFunctioningModemPresentUInt");
                    break;
                case 294:
                    System.out.println("Status: TimeOutAfterSendingMessageUInt");
                    break;
                case 295:
                    System.out.println("Status: SBD_MessageTimeOutByModemUInt");
                    break;
                case 296:
                    System.out.println("Status: SBD_MessageChecksumWrongUInt");
                    break;
                case 297:
                    System.out.println("Status: SBD_MessageSizeWrongUInt");
                    break;
                case 298:
                    System.out.println("Status: UnexpectedResponseAfterWritingDataToMobileOriginatedBufferUInt");
                    break;
                case 299:
                    System.out.println("Status: SBD_MessageSizeTooBigOrTooSmallUInt");
                    break;
                    
                // -- Status Return Codes (300 - 399) --
                    case 300:
                    System.out.println("Status: SuccessByteAfterSBDIX_UInt");
                    break;
                case 301:
                    System.out.println("Status: MessageSizeAcceptedUInt");
                    break;
                case 302:
                    System.out.println("Status: MO_BufferClearedSuccessfullyUInt");
                    break;
                case 303:
                    System.out.println("Status: MT_BufferClearedSuccessfullyUInt");
                    break;
                case 304:
                    System.out.println("Status: OK_FoundUInt");
                    break;
                case 305:
                    System.out.println("Status: RingIndicationDisabledUInt");
                    break;
                case 306:
                    System.out.println("Status: SetupSuccessfulUInt");
                    break;
                case 307:
                    System.out.println("Status: MT_MessageRetrievedCorrectlyUInt");
                    break;
                case 308:
                    System.out.println("Status: MT_MessageIsNullUInt");
                    break;
                case 309:
                    System.out.println("Status: ModemSetupSuccessfulUInt");
                    break;
                case 310:
                    System.out.println("Status: CorrectModemConnectedUInt");
                    break;
                case 311:
                    System.out.println("Status: idleUInt");
                    break;
                case 313:
                    System.out.println("Status: NetworkAvailableWithAcceptableSignalStrengthUInt");
                    break;
                case 314:
                    System.out.println("Status: VerifyDisableMT_AlertUInt");
                    break;
                case 315:
                    System.out.println("Status: FlushedUART_BufferUInt");
                    break;
                case 316:
                    System.out.println("Status: ArraySentToModemUInt");
                    break;
                case 317:
                    System.out.println("Status: InitiateTransmitAndReceiveUInt");
                    break;
                case 318:
                    System.out.println("Status: TellModemToClearMO_BufferUInt");
                    break;
                case 319:
                    System.out.println("Status: TellModemToClearMT_BufferUInt");
                    break;
                case 320:
                    System.out.println("Status: ToldModemToGiveUsTheReceivedMessageUInt");
                    break;
                case 322:
                    System.out.println("Status: TransmissionProcessHasBegunUInt");
                    break;
                case 323:
                    System.out.println("Status: SentPerformTransmitUInt");
                    break;
                case 324:
                    System.out.println("Status: SentgetReceivedDataUInt");
                    break;
                case 326:
                    System.out.println("Status: AboutToStartTransmitProcessUInt");
                    break;
                case 327:
                    System.out.println("Status: WaitingForOK_FromModemUInt");
                    break;
                case 328:
                    System.out.println("Status: InitialReturnCodeValueUInt");
                    break;
                case 329:
                    System.out.println("Status: InitialMPM_ResponseValueUInt");
                    break;
                case 330:
                    System.out.println("Status: BusySettingUpModemUInt");
                    break;
                case 331:
                    System.out.println("Status: PerformingPingUInt");
                    break;
                case 333:
                    System.out.println("Status: PingNotRunningUInt");
                    break;
                case 334:
                    System.out.println("Status: MPM_Busy_TransmitCommandPendingUInt");
                    break;
                case 335:
                    System.out.println("Status: ModemSetupProceedingUInt");
                    break;
                case 336:
                    System.out.println("Status: ModemDefaultsSetUInt");
                    break;
                case 337:
                    System.out.println("Status: SentPingUInt");
                    break;
                case 338:
                    System.out.println("Status: SBD_MessageSuccessfullyWrittenUInt");
                    break;
                case 339:
                    System.out.println("Status: MT_MessagePendingUInt");
                    break;
                case 340:
                    System.out.println("Status: MT_MessagesPendingUInt");
                    break;
                
                // -- Success Return Codes (400-499) --
                case 400:
                    System.out.println("Status: PingThroughMPM_AndModemSuccessUInt");
                    break;
                case 401:
                    System.out.println("Status: ModemReadyForUseUInt");
                    break;
                case 402:
                    System.out.println("Status: TransmitSuccessfulAndNoReceiveUInt");
                    break;
                case 403:
                    System.out.println("Status: TransmitAndReceiveSuccessfulUInt");
                    break;
                case 404:
                    System.out.println("Status: TransmitAndReceiveSuccessfulPlusReceivePendingUInt");
                    break;
                case 405:
                    System.out.println("Status: dataLoopAroundEnabledUInt");
                    break;
                case 406:
                    System.out.println("Status: dataLooopAroundDisabledUInt");
                    break;
                case 407:
                    System.out.println("Status: receiveDataPlacedInReceiveArrayUint");
                    break;

            }
            System.out.println();
    }
}