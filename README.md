
# ESP32 BLE Tracking System with ThingSpeak and LINE Notifications

This repository contains a complete solution for tracking BLE devices using an ESP32, visualizing their status on ThingSpeak, and sending notifications via LINE. It includes:

-   **ESP32 Firmware**: Scans nearby BLE devices, checks against a tracked list from Google Sheets, updates ThingSpeak channels, and triggers LINE notifications through Google Apps Script.
    
-   **Google Apps Script**:
    
    -   **Database Management** (interact with Google Sheets to store/delete tracked devices).
        
    -   **LINE Notification Handler** (broadcasts a message when a tracked device is out of range).
        
-   **Web UI**: A Bootstrap-based dashboard for end users to:
    
    -   Add or remove tracked BLE devices.
        
    -   View the list of tracked devices.
        
    -   Visualize each device’s online/offline status over time (using Chart.js and ThingSpeak data).
   
   ## Table of Contents

1.  [Features](#features)
    
2.  [System Architecture](#system-architecture)
    
3.  [Prerequisites](#prerequisites)
    
4.  [Getting Started](#getting-started)
    
    -   [1. Clone Repository](#1-clone-repository)
        
    -   [2. Configure Google Sheets & Apps Script](#2-configure-google-sheets--apps-script)
        
    -   [3. Create LINE Bot & Obtain Channel Token](#3-create-line-bot--obtain-channel-token)
        
    -   [4. Configure ThingSpeak Channel](#4-configure-thingspeak-channel)
        
    -   [5. Deploy Google Apps Script](#5-deploy-google-apps-script)
        
    -   [6. Configure and Upload ESP32 Firmware](#6-configure-and-upload-esp32-firmware)
        
    -   [7. Deploy Web UI](#7-deploy-web-ui)
        
5.  [Usage](#usage)
   
## Features

-   **BLE Scanning on ESP32**
    
    -   Actively scans for BLE beacon devices in a configurable interval.
        
    -   Compares scanned addresses against a dynamic “tracked devices” list fetched from Google Sheets.
        
    -   Filters by RSSI threshold to determine in-range vs. out-of-range status.
        
-   **Real-Time Status Updates via ThingSpeak**
    
    -   Updates a specified ThingSpeak channel and field (per device) with `1` (in range) or `0` (out of range).
        
    -   Visual historical data is available for each device through ThingSpeak’s JSON API.
        
-   **LINE Notifications**
    
    -   When a tracked device is not found or RSSI falls below threshold, the ESP32 triggers a Google Apps Script endpoint (GAS) to broadcast a LINE message:
        
        > “⚠️ Beacon {name} ({MAC}) is not in range!”
        
-   **Web Dashboard**
    
    -   Allows end users to **add** or **remove** BLE devices (specify MAC address, human-readable name, and ThingSpeak field number).
        
    -   Displays the current tracked list, with delete buttons.
        
    -   Renders historical “in-range / out-of-range” charts (per device) using Chart.js, fetching data from ThingSpeak.

 ## System Architecture
![image](https://github.com/WJH910529/ESP32-BLE-Tracking-System-with-ThingSpeak-and-LINE-Notifications/blob/16f9d62a85be1036ca6d3af263c49867679a4e08/system%20architecture.jpg)

1.  **ESP32**:
    
    -   Scans for BLE devices every 10 seconds.
        
    -   Fetches the current list of tracked devices (via a GET request to GAS “DB Management” endpoint).
        
    -   For each tracked MAC, checks if it’s found and RSSI ≥ threshold.
        
    -   Updates ThingSpeak (1 or 0) on the corresponding field.
        
    -   If out of range, invokes GAS “LINE Notify” endpoint to broadcast a LINE message.
        
2.  **Google Apps Script** (deployed as two separate web apps):
    
    -   **DB Management**:
        
        -   **GET**: Returns `JSON` of all tracked devices (MAC, name, ThingSpeak field).
            
        -   **POST**: Adds or deletes a row in Google Sheets (action = `delete` or default = `add`).
            
    -   **LINE Notify Handler**:
        
        -   **doGet(e) / doPost(e)**: Accepts `mac` & `name` parameters, constructs a LINE broadcast payload, and sends via the LINE Messaging API.
            
3.  **ThingSpeak**:
    
    -   A public (or private) channel with up to 8 fields. Each tracked device uses one field number to log `1` (present) or `0` (absent) on each scan cycle.
        
4.  **Web UI**:
    
    -   A Bootstrap + Chart.js single-page application (static HTML/JS) that:
        
        1.  Fetches the tracked devices list from GAS “DB Management” (`GET`).
            
        2.  Renders the list and allows “Add” / “Delete”.
            
        3.  Periodically fetches recent ThingSpeak data (via JSON API) to plot line charts per device (grouped by date).

## Prerequisites

Before you begin, you will need:

1.  **Hardware**
    
    -   An ESP32 development board.
        
    -   Power supply (USB cable).
        
    -   BLE beacons or BLE-advertising devices you wish to track.
        
2.  **Accounts & Services**
    
    -   A **Google Account** to host:
        
        -   Google Sheets (for the tracked-device database).
            
        -   Google Apps Script (for the two web apps).
            
    -   A **ThingSpeak** account:
        
        -   Create a channel with at least as many fields as you plan to track devices (max 8 fields).
            
    -   A **LINE Developer** account to set up:
        
        -   A Messaging API channel (for broadcasting “device out of range” notifications).
            
    -   A static-hosting solution or GitHub Pages to serve the Web UI (optional but recommended).
        
3.  **Software Tools**
    
    -   Arduino IDE (or PlatformIO) with ESP32 support installed.
        
    -   Node.js & npm (if you wish to run a local static server for development).
        
    -   A modern browser (Chrome/Firefox) for using the Web UI and viewing charts.

## Getting Started

Follow these steps to set up and run the project end-to-end.

### 1. Clone Repository
`git clone https://github.com/<your-username>/esp32-ble-tracker.git cd esp32-ble-tracker` 

### 2. Configure Google Sheets & Apps Script

1.  **Create a new Google Sheet** and rename the first sheet to `工作表1` (or update the Apps Script constants if you choose a different sheet name).
    
2.  **Add column headers** in row 1:
    `| MAC           | Name | Field |` 
    
    Row 2 onward will contain tracked devices (MAC, human-readable name, ThingSpeak field number).
    
3.  **Open Google Apps Script Editor**:
    
    -   In the same Google Sheet, go to **Extensions → Apps Script**.
        
    -   Delete any default `Code.gs` files and create two separate script files (e.g., `db-management.gs` and `line-notify.gs`).
        
4.  **Copy the following code** into **db-management.gs** (for device management):
    ```js
	const SHEET_NAME = '工作表1';

	// GET: Return all tracked devices as JSON
	function doGet(e) {
	  const sheet   = SpreadsheetApp.getActive().getSheetByName(SHEET_NAME);
	  const rows    = sheet.getDataRange().getValues();
	  const devices = rows.slice(1).map(r => ({
	    mac:   r[0],
	    name:  r[1],
	    field: Number(r[2])
	  }));
	  return ContentService
	    .createTextOutput(JSON.stringify({ devices }))
	    .setMimeType(ContentService.MimeType.JSON);
	}

	// POST: Add or delete a tracked device
	function doPost(e) {
	  const sheet = SpreadsheetApp.getActive().getSheetByName(SHEET_NAME);
	  const action = e.parameter.action;

	  if (action === 'delete') {
	    const mac = e.parameter.mac;
	    const data = sheet.getDataRange().getValues();
	    for (let i = data.length; i >= 2; i--) {
	      if (data[i-1][0] === mac) {
	        sheet.deleteRow(i);
	        break;
	      }
	    }
	    return ContentService
	      .createTextOutput(JSON.stringify({ result: 'deleted' }))
	      .setMimeType(ContentService.MimeType.JSON);
	  } else {
	    // Default: add a new device
	    const mac   = e.parameter.mac;
	    const name  = e.parameter.name;
	    const field = Number(e.parameter.field);
	    sheet.appendRow([ mac, name, field ]);
	    return ContentService
	      .createTextOutput(JSON.stringify({ result: 'OK' }))
	      .setMimeType(ContentService.MimeType.JSON);
	  }
	}
    ```
    
5.  **In the same Apps Script project**, create **line-notify.gs** and paste:
    ```js
	const LINE_BROADCAST_URL = 'https://api.line.me/v2/bot/message/broadcast';

	function getChannelToken() {
	  return PropertiesService
	    .getScriptProperties()
	    .getProperty('line_channel_access_token');
	}

	// Send a LINE broadcast with MAC and name of the missing device
	function handleBroadcast(mac, name) {
	  const token = getChannelToken();
	  const text  = `⚠️ Beacon ${name} (${mac}) is not in range!`;
	  const payload = { messages: [{ type: 'text', text }] };
	  const options = {
	    method: 'post',
	    contentType: 'application/json',
	    headers: { 'Authorization': 'Bearer ' + token },
	    payload: JSON.stringify(payload),
	    muteHttpExceptions: true
	  };
	  const resp = UrlFetchApp.fetch(LINE_BROADCAST_URL, options);
	  return ContentService
	    .createTextOutput(`LINE Broadcast HTTP ${resp.getResponseCode()}\n${resp.getContentText()}`);
	}

	// doGet and doPost both route to handleBroadcast
	function doGet(e) {
	  const mac  = e.parameter.mac  || '';
	  const name = e.parameter.name || '';
	  return handleBroadcast(mac, name);
	}

	function doPost(e) {
	  let data = {};
	  try { data = JSON.parse(e.postData.contents); } catch(_) {}
	  const mac  = data.mac  || '';
	  const name = data.name || '';
	  return handleBroadcast(mac, name);
	}
    ```
    
6.  **Set Up Script Properties**:
    
    -   In Apps Script editor, go to **Project Settings → Script Properties**.
        
    -   Add a key called `line_channel_access_token` with the value of your LINE channel’s access token (see next section).
        

### 3. Create LINE Bot & Obtain Channel Token

1.  Log in to the [LINE Developers Console.](https://developers.line.biz/)
    
2.  Create a new **Messaging API** channel under a provider (select “LINE Official Account”).
    
3.  From the channel settings, copy the **Channel Access Token**.
    
4.  In your Google Apps Script project, go to **Project Settings → Script Properties** and add `line_channel_access_token = <Your-Channel-Access-Token>`.
    

### 4. Configure ThingSpeak Channel

1.  Log in to [ThingSpeak](https://thingspeak.com/) and create a new channel (or use an existing one).
    
2.  In **Channel Settings**, give it a name (e.g., “BLE Tracker”).
    
3.  Enable up to **8 fields** (each tracked device will correspond to a field ID between 1 and 8).
    
4.  Copy the **Write API Key** for this channel (you will paste it into the ESP32 firmware).
    
5.  Note your **Channel ID** if you want to query feeds in the Web UI.
    

### 5. Deploy Google Apps Script

1.  In the Apps Script editor, select **Deploy → New deployment**.
    
2.  Choose **“Web app”**.
    
3.  Under “Execute as,” select **Me (your account)**.
    
4.  Under “Who has access,” choose **Anyone** (so the ESP32 and the Web UI can call it).
    
5.  Click **Deploy**, and copy the **Web App URL** for both scripts:
    
    -   One URL for **db-management.gs**
        
    -   Another URL for **line-notify.gs**
        
6.  You will use these two URLs in your ESP32 code (see Section 6).
    

### 6. Configure and Upload ESP32 Firmware

1.  Open the `esp32/ble_tracker.ino` file in the Arduino IDE (or PlatformIO).
    
2.  **Update WiFi Credentials**:
    ```c
	    const char* ssid     = "YOUR_SSID";
		const char* password = "YOUR_PASSWORD";
    ```
    
3.  **Update ThingSpeak Settings**:
    ```c
		const char* TS_API_KEY = "YOUR_THINGSPEAK_WRITE_KEY";
		const String tsBaseUrl = String("http://api.thingspeak.com/update?api_key=") + TS_API_KEY;
    ```
    
4.  **Update Google Apps Script URLs**:
    ```c
	    const String gasUrlForLine     = "https://script.google.com/macros/s/XXXXX/exec"; // line-notify URL
		const String gasUrlForDatabase = "https://script.google.com/macros/s/YYYYY/exec"; // db-management URL
    ``` 
    
5.  (Optional) **RSSI Threshold**:
    
    -   By default, `RSSIThreshold = -80`. Adjust if needed to tighten/loosen “in-range” criteria.
        
6.  Save and upload the firmware to your ESP32 board.
    
7.  Open the Serial Monitor at `115200 baud` to observe:
    
    -   WiFi connection status
        
    -   BLE scan results
        
    -   ThingSpeak update responses
        
    -   LINE-notify HTTP responses
        

### 7. Deploy Web UI

1.  The `index.html` file is a standalone HTML page (Bootstrap + Chart.js). You can serve it in two ways:
    
    -   **GitHub Pages**: 
    -   **Local static server** (for testing)
2.  **Update Endpoint URLs** inside `index.html` (at top of script):
    ```c
	    const GAS_URL = 'https://script.google.com/macros/s/YYYYY/exec';                     // same as gasUrlForDatabase
		const TS_READ_URL = 'https://api.thingspeak.com/channels/<<CHANNEL_ID>>/feeds.json?api_key=YOUR_READ_API_KEY&results=100';
    ```
    
3.  Open the page in your browser:
    
4.  The dashboard will automatically:
    
    -   Fetch the tracked devices list (mac, name, field).
        
    -   Render the list on the left.
        
    -   Plot line charts for the selected device’s ThingSpeak data showing daily in-range (1) vs. out-of-range (0) over time.
        
    -   Allow adding a new tracked device (MAC, name, ThingSpeak field).
        
    -   Allow deleting an existing device (with confirmation modal).
        

## Usage

1.  **Add a New Tracked Device**
    
    -   Open the Web UI.
        
    -   Under “新增追蹤裝置,” enter:
        
        -   **MAC Address** (format: `AA:BB:CC:DD:EE:FF`)
            
        -   **Device Name** (e.g., “Office Beacon”)
            
        -   **ThingSpeak Field ID** (an integer between 1–8, one per device).
            
    -   Click **新增**. The UI will POST to GAS → Google Sheets will append a row → The Web UI updates the list.
        
2.  **Delete a Tracked Device**
    
    -   In the “追蹤清單,” click the red **刪除** button next to the device.
        
    -   Confirm deletion in the modal. The UI will call GAS with `action=delete&mac=<MAC>`.
        
    -   The row in Google Sheets is removed, and the Web UI list refreshes.
        
3.  **ESP32 BLE Scan & Reporting**
    
    -   Every 10 seconds, the ESP32:
        
        -   Fetches `GET https://script.google.com/macros/s/YYYYY/exec` → gets a JSON array of tracked devices.
            
        -   Scans 10 seconds for BLE → matches scanned MACs against the list.
            
        -   If `found && rssi ≥ RSSIThreshold`, sets `value = 1`; otherwise `value = 0`.
            
        -   Calls `http://api.thingspeak.com/update?api_key=YOUR_WRITE_KEY&field<field>=<value>`.
            
        -   If `value == 0`, calls `https://script.google.com/macros/s/XXXXX/exec?mac=<MAC>&name=<URL_ENCODED_NAME>&status=not_found` → triggers LINE broadcast.
            
4.  **View Historical Status**
    
    -   In the Web UI, click any device in the “追蹤清單.”
        
    -   The right-hand chart area will show one or more line charts (grouped by date) plotting “0/1” status over time for that device.
        
    -   The Web UI fetches the latest 100 ThingSpeak entries from `TS_READ_URL` and filters by the selected field (e.g., `field3`).
