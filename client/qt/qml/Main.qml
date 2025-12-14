import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 800
    minimumWidth: 800
    minimumHeight: 600
    title: qsTr("Face Tracker")

    // Fluent Design Colors
    readonly property color backgroundColor: "#F3F3F3"
    readonly property color surfaceColor: "#FFFFFF"
    readonly property color cardColor: "#FAFAFA"
    readonly property color accentColor: "#0078D4"
    readonly property color accentHover: "#106EBE"
    readonly property color accentPressed: "#005A9E"
    readonly property color textPrimary: "#1F1F1F"
    readonly property color textSecondary: "#605E5C"
    readonly property color textTertiary: "#8A8886"
    readonly property color dividerColor: "#E1DFDD"
    readonly property color layerColor: Qt.rgba(0, 0, 0, 0.05)
    readonly property color successColor: "#107C10"
    readonly property color warningColor: "#F7630C"
    readonly property color errorColor: "#D13438"

    // Theme support
    property bool darkMode: settings ? settings.darkMode : false
    readonly property color themeBackgroundColor: darkMode ? "#1E1E1E" : backgroundColor
    readonly property color themeSurfaceColor: darkMode ? "#2D2D2D" : surfaceColor
    readonly property color themeCardColor: darkMode ? "#252525" : cardColor
    readonly property color themeTextPrimary: darkMode ? "#FFFFFF" : textPrimary
    readonly property color themeTextSecondary: darkMode ? "#B3B3B3" : textSecondary
    readonly property color themeDividerColor: darkMode ? "#3F3F3F" : dividerColor

    // Connection state enum values
    readonly property int connectionStateDisconnected: 0
    readonly property int connectionStateConnecting: 1
    readonly property int connectionStateConnected: 2
    readonly property int connectionStateError: 3

    // Connection state
    property int connectionState: backend ? backend.connectionState : connectionStateDisconnected
    property string connectionErrorMessage: backend ? backend.connectionErrorMessage : ""
    property var availableDevices: backend ? backend.availableDevices : []

    // Properties from backend
    property real currentFps: backend ? backend.fps : 0
    property int framesProcessed: backend ? backend.framesProcessed : 0
    property int facesDetected: backend ? backend.facesDetected : 0
    property string currentCamera: backend ? backend.currentCamera : ""
    property int currentModelType: backend ? backend.currentModelType : 0
    property bool settingsVisible: false

    // Display properties from settings
    property bool cameraPreviewVisible: settings ? settings.cameraPreviewVisible : true
    property bool showBoundingBoxes: settings ? settings.showBoundingBoxes : true
    property bool showConfidence: settings ? settings.showConfidence : true
    property bool showDistance: settings ? settings.showDistance : true

    // Connect QML signals to backend slots
    Component.onCompleted: {
        if (backend) {
            cameraSelected.connect(backend.selectCamera)
            modelSelected.connect(backend.selectModel)
            settingsChanged.connect(backend.applySettings)
            quitRequested.connect(backend.quitRequested)
            connectRequested.connect(backend.connectToDevice)
            disconnectRequested.connect(backend.disconnectFromDevice)
            scanRequested.connect(backend.scanForDevices)
            calibrateRequested.connect(backend.calibrateDevice)
        }

        // Load settings from storage
        if (settings) {
            settings.load()

            // Apply all settings from storage
            darkMode = settings.darkMode
            cameraPreviewVisible = settings.cameraPreviewVisible
            showBoundingBoxes = settings.showBoundingBoxes
            showConfidence = settings.showConfidence
            showDistance = settings.showDistance

            // Start timer to apply settings after backend is fully ready
            settingsApplyTimer.start()
        }
    }

    // Timer to ensure backend is ready before applying settings
    Timer {
        id: settingsApplyTimer
        interval: 500
        repeat: false
        onTriggered: {
            if (settings && backend) {
                console.log("Applying initial settings: FPS=" + settings.targetFps)
                root.settingsChanged({
                    "fps": settings.targetFps,
                    "throttling": settings.throttlingEnabled,
                    "confidence": settings.confidenceThreshold,
                    "nms": settings.nmsThreshold,
                    "gpu": settings.gpuEnabled,
                    "verbose": settings.verboseLogging
                })
            }
        }
    }

    // Signals to communicate with C++
    signal cameraSelected(string deviceId)
    signal modelSelected(int modelType)
    signal settingsChanged(var settings)
    signal quitRequested()
    signal connectRequested(string deviceAddress)
    signal disconnectRequested()
    signal scanRequested()
    signal calibrateRequested()

    color: themeBackgroundColor

    // Connection state change handler
    onConnectionStateChanged: {
        if (connectionState === connectionStateConnecting) {
            connectingPopup.open()
        } else if (connectionState === connectionStateConnected) {
            connectingPopup.close()
            connectionSuccessPopup.open()
        } else if (connectionState === connectionStateError) {
            connectingPopup.close()
            connectionErrorPopup.open()
        } else if (connectionState === connectionStateDisconnected) {
            connectingPopup.close()
        }
    }

    // Main layout
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Title bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: themeSurfaceColor

            // Shadow effect using MultiEffect
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.1)
                shadowVerticalOffset: 2
                shadowBlur: 0.3
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 16

                // App icon and title
                Rectangle {
                    width: 32
                    height: 32
                    radius: 4
                    color: accentColor

                    Text {
                        anchors.centerIn: parent
                        text: "FT"
                        font.pixelSize: 14
                        font.bold: true
                        font.family: "Segoe UI"
                        color: "white"
                    }
                }

                Label {
                    text: qsTr("Face Tracker")
                    font.pixelSize: 16
                    font.family: "Segoe UI"
                    font.weight: Font.DemiBold
                    color: themeTextPrimary
                }

                Rectangle {
                    width: 1
                    height: 24
                    color: themeDividerColor
                }

                // Connection button
                Button {
                    id: connectionButton
                    text: {
                        switch (root.connectionState) {
                            case root.connectionStateDisconnected:
                                return "Connect"
                            case root.connectionStateConnecting:
                                return "Connecting..."
                            case root.connectionStateConnected:
                                return "Connected"
                            case root.connectionStateError:
                                return "Retry"
                            default:
                                return "Connect"
                        }
                    }
                    font.pixelSize: 13
                    font.family: "Segoe UI"
                    font.weight: Font.DemiBold
                    enabled: root.connectionState !== root.connectionStateConnecting

                    contentItem: Row {
                        spacing: 8
                        anchors.centerIn: parent

                        // Connection status indicator
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: {
                                switch (root.connectionState) {
                                    case root.connectionStateDisconnected:
                                        return root.textTertiary
                                    case root.connectionStateConnecting:
                                        return root.warningColor
                                    case root.connectionStateConnected:
                                        return root.successColor
                                    case root.connectionStateError:
                                        return root.errorColor
                                    default:
                                        return root.textTertiary
                                }
                            }

                            // Pulsing animation when connecting
                            SequentialAnimation on opacity {
                                running: root.connectionState === root.connectionStateConnecting
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.3; duration: 500 }
                                NumberAnimation { to: 1.0; duration: 500 }
                            }
                        }

                        Text {
                            text: connectionButton.text
                            font: connectionButton.font
                            color: root.connectionState === root.connectionStateConnected ? root.successColor : "white"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    background: Rectangle {
                        color: {
                            if (!connectionButton.enabled) return root.textTertiary
                            if (root.connectionState === root.connectionStateConnected) {
                                return connectionButton.pressed ? Qt.darker(root.successColor, 1.2) :
                                       connectionButton.hovered ? Qt.lighter(root.successColor, 1.1) :
                                       Qt.rgba(root.successColor.r, root.successColor.g, root.successColor.b, 0.2)
                            }
                            return connectionButton.pressed ? root.accentPressed :
                                   connectionButton.hovered ? root.accentHover : root.accentColor
                        }
                        radius: 4
                        border.width: root.connectionState === root.connectionStateConnected ? 1 : 0
                        border.color: root.successColor
                    }

                    onClicked: {
                        if (root.connectionState === root.connectionStateConnected) {
                            disconnectConfirmPopup.open()
                        } else {
                            deviceSelectionPopup.open()
                        }
                    }
                }

                Rectangle {
                    width: 1
                    height: 24
                    color: themeDividerColor
                    visible: root.connectionState === root.connectionStateConnected
                }

                // Status pills (only visible when connected)
                Row {
                    spacing: 12
                    Layout.fillWidth: true
                    visible: root.connectionState === root.connectionStateConnected

                    StatusPill {
                        label: "FPS"
                        value: root.currentFps.toFixed(1)
                        statusColor: root.currentFps >= 25 ? successColor : warningColor
                    }

                    StatusPill {
                        label: "Frames"
                        value: root.framesProcessed.toString()
                        statusColor: themeTextSecondary
                    }

                    StatusPill {
                        label: "Faces"
                        value: root.facesDetected.toString()
                        statusColor: root.facesDetected > 0 ? successColor : themeTextSecondary
                    }
                }

                // Calibrate button (only visible when connected)
                Button {
                    text: "Calibrate"
                    flat: true
                    font.pixelSize: 13
                    font.family: "Segoe UI"
                    visible: root.connectionState === root.connectionStateConnected

                    contentItem: Row {
                        spacing: 6

                        Rectangle {
                            width: 16
                            height: 16
                            anchors.verticalCenter: parent.verticalCenter
                            color: "transparent"

                            Canvas {
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);
                                    ctx.strokeStyle = root.darkMode ? "white" : textSecondary;
                                    ctx.lineWidth = 1.5;

                                    // Target icon
                                    ctx.beginPath();
                                    ctx.arc(8, 8, 6, 0, 2 * Math.PI);
                                    ctx.stroke();
                                    ctx.beginPath();
                                    ctx.arc(8, 8, 3, 0, 2 * Math.PI);
                                    ctx.stroke();
                                    ctx.beginPath();
                                    ctx.moveTo(8, 2);
                                    ctx.lineTo(8, 5);
                                    ctx.moveTo(8, 11);
                                    ctx.lineTo(8, 14);
                                    ctx.moveTo(2, 8);
                                    ctx.lineTo(5, 8);
                                    ctx.moveTo(11, 8);
                                    ctx.lineTo(14, 8);
                                    ctx.stroke();
                                }
                            }
                        }

                        Text {
                            text: "Calibrate"
                            color: root.darkMode ? "white" : textPrimary
                            font.pixelSize: 13
                            font.family: "Segoe UI"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    background: Rectangle {
                        color: {
                            if (parent.pressed) return Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.3)
                            if (parent.hovered) return Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.1)
                            return "transparent"
                        }
                        radius: 4
                    }

                    onClicked: {
                        root.calibrateRequested()
                    }
                }

                Item { Layout.fillWidth: true }

                // Settings button
                Button {
                    text: "Settings"
                    flat: true
                    font.pixelSize: 13
                    font.family: "Segoe UI"

                    contentItem: Row {
                        spacing: 6

                        Rectangle {
                            width: 16
                            height: 16
                            anchors.verticalCenter: parent.verticalCenter
                            color: "transparent"

                            Canvas {
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);
                                    ctx.strokeStyle = root.darkMode ? "white" : textSecondary;
                                    ctx.lineWidth = 1.5;

                                    // Gear icon
                                    ctx.beginPath();
                                    ctx.arc(8, 8, 5, 0, 2 * Math.PI);
                                    ctx.stroke();
                                    ctx.beginPath();
                                    ctx.arc(8, 8, 2, 0, 2 * Math.PI);
                                    ctx.stroke();
                                }
                            }
                        }

                        Text {
                            text: "Settings"
                            font: parent.parent.font
                            color: themeTextPrimary
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    onClicked: settingsDrawer.open()

                    background: Rectangle {
                        color: parent.hovered ? layerColor : "transparent"
                        radius: 4
                    }
                }
            }
        }

        // Main content area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 16

            // Welcome/Connect screen (shown when disconnected)
            Rectangle {
                id: welcomeScreen
                anchors.fill: parent
                color: themeSurfaceColor
                radius: 8
                visible: root.connectionState !== root.connectionStateConnected

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, 0.12)
                    shadowVerticalOffset: 2
                    shadowBlur: 0.4
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 24

                    // Icon
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 80
                        height: 80
                        radius: 40
                        color: root.darkMode ? "#3A3A3A" : root.layerColor

                        Canvas {
                            anchors.centerIn: parent
                            width: 40
                            height: 40

                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.clearRect(0, 0, width, height);
                                ctx.strokeStyle = root.accentColor;
                                ctx.lineWidth = 2;

                                // Bluetooth icon
                                ctx.beginPath();
                                ctx.moveTo(20, 5);
                                ctx.lineTo(30, 15);
                                ctx.lineTo(20, 25);
                                ctx.lineTo(20, 35);
                                ctx.lineTo(30, 25);
                                ctx.lineTo(10, 15);
                                ctx.moveTo(30, 15);
                                ctx.lineTo(10, 25);
                                ctx.moveTo(20, 5);
                                ctx.lineTo(20, 35);
                                ctx.stroke();
                            }
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Connect to Device")
                        font.pixelSize: 24
                        font.family: "Segoe UI"
                        font.weight: Font.DemiBold
                        color: root.themeTextPrimary
                    }

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Connect to your ESP32 device via Bluetooth\nto start face tracking")
                        font.pixelSize: 14
                        font.family: "Segoe UI"
                        color: root.themeTextSecondary
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 44
                        text: root.connectionState === root.connectionStateConnecting ? "Connecting..." : "Connect Device"
                        font.pixelSize: 14
                        font.family: "Segoe UI"
                        font.weight: Font.DemiBold
                        enabled: root.connectionState !== root.connectionStateConnecting

                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.enabled ? (parent.pressed ? root.accentPressed : parent.hovered ? root.accentHover : root.accentColor) : root.textTertiary
                            radius: 6
                        }

                        onClicked: deviceSelectionPopup.open()
                    }
                }
            }

            // Video card (shown when connected)
            Rectangle {
                id: videoCard
                anchors.fill: parent
                color: surfaceColor
                radius: 8
                visible: root.connectionState === root.connectionStateConnected

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, 0.12)
                    shadowVerticalOffset: 2
                    shadowBlur: 0.4
                }

                // Video display
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 16
                    color: root.darkMode ? "#1A1A1A" : "#000000"
                    radius: 4
                    clip: true

                    // Placeholder
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 16
                        visible: !videoImage.hasContent

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 64
                            height: 64
                            radius: 32
                            color: root.darkMode ? "#3A3A3A" : layerColor

                            Canvas {
                                anchors.centerIn: parent
                                width: 32
                                height: 32

                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);
                                    ctx.strokeStyle = root.darkMode ? "#B3B3B3" : textTertiary;
                                    ctx.lineWidth = 2;

                                    // Camera icon
                                    ctx.strokeRect(4, 8, 24, 16);
                                    ctx.beginPath();
                                    ctx.arc(16, 16, 4, 0, 2 * Math.PI);
                                    ctx.stroke();
                                }
                            }
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Waiting for camera...")
                            font.pixelSize: 14
                            font.family: "Segoe UI"
                            color: themeTextSecondary
                        }

                        ProgressBar {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 200
                            indeterminate: true

                            background: Rectangle {
                                color: root.darkMode ? "#3A3A3A" : layerColor
                                radius: 2
                            }

                            contentItem: Rectangle {
                                color: accentColor
                                radius: 2
                            }
                        }
                    }

                    // Video frame
                    Image {
                        id: videoImage
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectFit
                        visible: root.cameraPreviewVisible
                        cache: false

                        property bool hasContent: status === Image.Ready && source != ""
                    }

                    // Face overlay
                    Canvas {
                        id: faceOverlay
                        anchors.fill: videoImage
                        visible: videoImage.hasContent && root.cameraPreviewVisible

                        property var faces: []

                        // Force repaint when display options change
                        Connections {
                            target: root
                            function onShowBoundingBoxesChanged() { faceOverlay.requestPaint() }
                            function onShowConfidenceChanged() { faceOverlay.requestPaint() }
                            function onShowDistanceChanged() { faceOverlay.requestPaint() }
                        }

                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.reset();

                            if (!faces || faces.length === 0) return;

                            var scaleX = width / videoImage.sourceSize.width;
                            var scaleY = height / videoImage.sourceSize.height;
                            var scale = Math.min(scaleX, scaleY);
                            var offsetX = (width - videoImage.sourceSize.width * scale) / 2;
                            var offsetY = (height - videoImage.sourceSize.height * scale) / 2;

                            for (var i = 0; i < faces.length; i++) {
                                var face = faces[i];
                                var x = face.x * scale + offsetX;
                                var y = face.y * scale + offsetY;
                                var w = face.width * scale;
                                var h = face.height * scale;

                                // Draw bounding box (only if enabled)
                                if (root.showBoundingBoxes) {
                                    ctx.strokeStyle = root.accentColor;
                                    ctx.lineWidth = 2;
                                    ctx.strokeRect(x, y, w, h);

                                    // Draw corners
                                    ctx.lineWidth = 3;
                                    var cornerSize = 12;

                                    // Top-left
                                    ctx.beginPath();
                                    ctx.moveTo(x, y + cornerSize);
                                    ctx.lineTo(x, y);
                                    ctx.lineTo(x + cornerSize, y);
                                    ctx.stroke();

                                    // Top-right
                                    ctx.beginPath();
                                    ctx.moveTo(x + w - cornerSize, y);
                                    ctx.lineTo(x + w, y);
                                    ctx.lineTo(x + w, y + cornerSize);
                                    ctx.stroke();

                                    // Bottom-left
                                    ctx.beginPath();
                                    ctx.moveTo(x, y + h - cornerSize);
                                    ctx.lineTo(x, y + h);
                                    ctx.lineTo(x + cornerSize, y + h);
                                    ctx.stroke();

                                    // Bottom-right
                                    ctx.beginPath();
                                    ctx.moveTo(x + w - cornerSize, y + h);
                                    ctx.lineTo(x + w, y + h);
                                    ctx.lineTo(x + w, y + h - cornerSize);
                                    ctx.stroke();
                                }

                                // Build label text based on enabled options
                                var labelParts = [];
                                labelParts.push("Face " + face.trackId);

                                if (root.showConfidence && face.confidence !== undefined) {
                                    labelParts.push(Math.round(face.confidence * 100) + "%");
                                }

                                if (root.showDistance && face.distance !== undefined) {
                                    labelParts.push(face.distance.toFixed(2) + "m");
                                }

                                // Draw label if any parts are visible
                                if (labelParts.length > 0) {
                                    var labelText = labelParts.join(" • ");
                                    ctx.font = "12px Segoe UI";
                                    var textWidth = ctx.measureText(labelText).width;

                                    ctx.fillStyle = root.accentColor;
                                    ctx.fillRect(x, y - 26, textWidth + 16, 24);

                                    ctx.fillStyle = "white";
                                    ctx.fillText(labelText, x + 8, y - 8);
                                }
                            }
                        }

                        function updateFaces(faceData) {
                            faces = faceData;
                            requestPaint();
                        }
                    }
                }
            }
        }
    }

    // Device Selection Popup
    Popup {
        id: deviceSelectionPopup
        anchors.centerIn: parent
        width: 400
        height: 450
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: root.themeSurfaceColor
            radius: 8
            border.color: root.themeDividerColor
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: qsTr("Select Device")
                    font.pixelSize: 18
                    font.family: "Segoe UI"
                    font.weight: Font.DemiBold
                    color: root.themeTextPrimary
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "✕"
                    flat: true
                    font.pixelSize: 14
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28

                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: root.themeTextPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: parent.hovered ? root.layerColor : "transparent"
                        radius: 4
                    }

                    onClicked: deviceSelectionPopup.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: root.themeDividerColor
            }

            // Scan button
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: scanningIndicator.running ? "Scanning..." : "Scan for Devices"
                font.pixelSize: 13
                font.family: "Segoe UI"

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8

                    BusyIndicator {
                        id: scanningIndicator
                        width: 16
                        height: 16
                        running: false
                        visible: running
                    }

                    Text {
                        text: parent.parent.text
                        font: parent.parent.font
                        color: root.themeTextPrimary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                background: Rectangle {
                    color: parent.pressed ? root.layerColor : parent.hovered ? Qt.lighter(root.layerColor, 1.5) : "transparent"
                    border.color: root.themeDividerColor
                    border.width: 1
                    radius: 4
                }

                onClicked: {
                    scanningIndicator.running = true
                    root.scanRequested()
                    scanTimer.start()
                }
            }

            Timer {
                id: scanTimer
                interval: 5000
                onTriggered: scanningIndicator.running = false
            }

            // Device list
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: deviceListView
                    model: root.availableDevices
                    spacing: 4

                    delegate: Rectangle {
                        width: deviceListView.width
                        height: 56
                        color: deviceMouseArea.containsMouse ? root.layerColor : "transparent"
                        radius: 4

                        MouseArea {
                            id: deviceMouseArea
                            anchors.fill: parent
                            hoverEnabled: true

                            onClicked: {
                                deviceSelectionPopup.close()
                                root.connectRequested(modelData.address)
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 12

                            Rectangle {
                                width: 32
                                height: 32
                                radius: 16
                                color: root.accentColor

                                Canvas {
                                    anchors.centerIn: parent
                                    width: 16
                                    height: 16

                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.clearRect(0, 0, width, height);
                                        ctx.strokeStyle = "white";
                                        ctx.lineWidth = 1.5;

                                        // Bluetooth icon
                                        ctx.beginPath();
                                        ctx.moveTo(8, 2);
                                        ctx.lineTo(12, 6);
                                        ctx.lineTo(8, 10);
                                        ctx.lineTo(8, 14);
                                        ctx.lineTo(12, 10);
                                        ctx.lineTo(4, 6);
                                        ctx.moveTo(12, 6);
                                        ctx.lineTo(4, 10);
                                        ctx.moveTo(8, 2);
                                        ctx.lineTo(8, 14);
                                        ctx.stroke();
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: modelData.name || "Unknown Device"
                                    font.pixelSize: 14
                                    font.family: "Segoe UI"
                                    font.weight: Font.Medium
                                    color: root.themeTextPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: modelData.address || ""
                                    font.pixelSize: 11
                                    font.family: "Segoe UI"
                                    color: root.themeTextSecondary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            Label {
                                text: "→"
                                font.pixelSize: 16
                                color: root.themeTextTertiary
                            }
                        }
                    }

                    // Empty state
                    Label {
                        anchors.centerIn: parent
                        text: qsTr("No devices found.\nTap 'Scan for Devices' to search.")
                        font.pixelSize: 13
                        font.family: "Segoe UI"
                        color: root.themeTextSecondary
                        horizontalAlignment: Text.AlignHCenter
                        visible: deviceListView.count === 0 && !scanningIndicator.running
                    }
                }
            }
        }
    }

    // Connecting Popup
    Popup {
        id: connectingPopup
        anchors.centerIn: parent
        width: 280
        height: 160
        modal: true
        closePolicy: Popup.NoAutoClose

        background: Rectangle {
            color: root.themeSurfaceColor
            radius: 8
            border.color: root.themeDividerColor
            border.width: 1
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 16

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                width: 48
                height: 48
                running: connectingPopup.visible
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Connecting...")
                font.pixelSize: 16
                font.family: "Segoe UI"
                font.weight: Font.DemiBold
                color: root.themeTextPrimary
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Please wait while connecting to device")
                font.pixelSize: 12
                font.family: "Segoe UI"
                color: root.themeTextSecondary
            }
        }
    }

    // Connection Success Popup
    Popup {
        id: connectionSuccessPopup
        anchors.centerIn: parent
        width: 280
        height: 180
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: root.themeSurfaceColor
            radius: 8
            border.color: root.successColor
            border.width: 2
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 16

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 48
                height: 48
                radius: 24
                color: Qt.rgba(root.successColor.r, root.successColor.g, root.successColor.b, 0.2)

                Label {
                    anchors.centerIn: parent
                    text: "✓"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    color: root.successColor
                }
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Connected!")
                font.pixelSize: 16
                font.family: "Segoe UI"
                font.weight: Font.DemiBold
                color: root.themeTextPrimary
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Device connected successfully")
                font.pixelSize: 12
                font.family: "Segoe UI"
                color: root.themeTextSecondary
            }
        }

        Timer {
            running: connectionSuccessPopup.visible
            interval: 2000
            onTriggered: connectionSuccessPopup.close()
        }
    }

    // Connection Error Popup
    Popup {
        id: connectionErrorPopup
        anchors.centerIn: parent
        width: 320
        height: 200
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: root.themeSurfaceColor
            radius: 8
            border.color: root.errorColor
            border.width: 2
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 16

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 48
                height: 48
                radius: 24
                color: Qt.rgba(root.errorColor.r, root.errorColor.g, root.errorColor.b, 0.2)

                Label {
                    anchors.centerIn: parent
                    text: "✕"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    color: root.errorColor
                }
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Connection Failed")
                font.pixelSize: 16
                font.family: "Segoe UI"
                font.weight: Font.DemiBold
                color: root.themeTextPrimary
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: root.connectionErrorMessage || qsTr("Unable to connect to device")
                font.pixelSize: 12
                font.family: "Segoe UI"
                color: root.themeTextSecondary
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                Layout.maximumWidth: 280
            }

            Button {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Try Again")
                font.pixelSize: 13
                font.family: "Segoe UI"

                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: parent.pressed ? root.accentPressed : parent.hovered ? root.accentHover : root.accentColor
                    radius: 4
                }

                onClicked: {
                    connectionErrorPopup.close()
                    deviceSelectionPopup.open()
                }
            }
        }
    }

    // Disconnect Confirmation Popup
    Popup {
        id: disconnectConfirmPopup
        anchors.centerIn: parent
        width: 300
        height: 160
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: root.themeSurfaceColor
            radius: 8
            border.color: root.themeDividerColor
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Disconnect from device?")
                font.pixelSize: 16
                font.family: "Segoe UI"
                font.weight: Font.DemiBold
                color: root.themeTextPrimary
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("This will stop face tracking")
                font.pixelSize: 13
                font.family: "Segoe UI"
                color: root.themeTextSecondary
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 12

                Button {
                    text: qsTr("Cancel")
                    font.pixelSize: 13
                    font.family: "Segoe UI"
                    Layout.preferredWidth: 100

                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: root.themeTextPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: parent.pressed ? root.layerColor : parent.hovered ? Qt.lighter(root.layerColor, 1.5) : "transparent"
                        border.color: root.themeDividerColor
                        border.width: 1
                        radius: 4
                    }

                    onClicked: disconnectConfirmPopup.close()
                }

                Button {
                    text: qsTr("Disconnect")
                    font.pixelSize: 13
                    font.family: "Segoe UI"
                    Layout.preferredWidth: 100

                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(root.errorColor, 1.2) : parent.hovered ? Qt.lighter(root.errorColor, 1.1) : root.errorColor
                        radius: 4
                    }

                    onClicked: {
                        disconnectConfirmPopup.close()
                        root.disconnectRequested()
                    }
                }
            }
        }
    }

    // Settings drawer
    Drawer {
        id: settingsDrawer
        width: 380
        height: root.height
        edge: Qt.RightEdge

        background: Rectangle {
            color: themeBackgroundColor
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Header
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                color: themeSurfaceColor

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 12

                    Label {
                        text: qsTr("Settings")
                        font.pixelSize: 20
                        font.family: "Segoe UI"
                        font.weight: Font.DemiBold
                        color: themeTextPrimary
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "✕"
                        flat: true
                        font.pixelSize: 16
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32

                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: themeTextPrimary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.hovered ? layerColor : "transparent"
                            radius: 4
                        }

                        onClicked: settingsDrawer.close()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: themeDividerColor
            }

            // Settings content (tabbed)
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: 0

                // Basic Settings Tab
                ScrollView {
                    clip: true

                    ColumnLayout {
                        width: settingsDrawer.width - 40
                        spacing: 20

                        Item { height: 10 }

                        SettingsGroup {
                            title: "Camera"

                            SettingDropdown {
                                id: settingsCameraCombo
                                label: "Camera Device"
                                Layout.leftMargin: 20
                                Layout.rightMargin: 20
                                model: cameraModel
                                textRole: "display"
                                currentIndex: settingsCameraCombo.currentIndex

                                onCurrentIndexChanged: {
                                    if (currentIndex >= 0 && cameraModel.count > 0) {
                                        var deviceId = cameraModel.get(currentIndex).deviceId;
                                        root.cameraSelected(deviceId);
                                    }
                                }
                            }

                            SettingDropdown {
                                label: "Resolution"
                                Layout.leftMargin: 20
                                Layout.rightMargin: 20
                                model: ["640×480", "1280×720", "1920×1080"]
                                currentIndex: {
                                    if (settings && settings.resolutionWidth === 1280 && settings.resolutionHeight === 720) return 1
                                    if (settings && settings.resolutionWidth === 1920 && settings.resolutionHeight === 1080) return 2
                                    return 0
                                }

                                onCurrentIndexChanged: {
                                    var resolutions = [[640, 480], [1280, 720], [1920, 1080]];
                                    if (settings && currentIndex >= 0 && currentIndex < resolutions.length) {
                                        settings.resolutionWidth = resolutions[currentIndex][0]
                                        settings.resolutionHeight = resolutions[currentIndex][1]
                                        root.settingsChanged({
                                            "width": resolutions[currentIndex][0],
                                            "height": resolutions[currentIndex][1]
                                        });
                                    }
                                }
                            }

                            SettingSlider {
                                id: targetFpsSlider
                                label: "Target FPS"
                                from: 1
                                to: 60
                                value: settings ? settings.targetFps : 30
                                stepSize: 1

                                onValueChanged: {
                                    var roundedValue = Math.round(value)
                                    if (settings && roundedValue !== settings.targetFps) {
                                        settings.targetFps = roundedValue
                                        root.settingsChanged({ "fps": roundedValue })
                                    }
                                }
                            }

                            SettingToggle {
                                id: throttlingToggle
                                label: "Enable Frame Throttling"
                                description: "Limit frame processing rate to target FPS"
                                checked: settings ? settings.throttlingEnabled : true

                                onCheckedChanged: {
                                    if (settings && checked !== settings.throttlingEnabled) {
                                        settings.throttlingEnabled = checked
                                        root.settingsChanged({ "throttling": checked })
                                    }
                                }
                            }
                        }

                        SettingsGroup {
                            title: "Display"

                            SettingToggle {
                                label: "Show Camera Preview"
                                description: "Display video feed"
                                checked: root.cameraPreviewVisible

                                onCheckedChanged: {
                                    root.cameraPreviewVisible = checked
                                    if (settings && checked !== settings.cameraPreviewVisible) {
                                        settings.cameraPreviewVisible = checked
                                    }
                                }
                            }

                            SettingToggle {
                                label: "Show Bounding Boxes"
                                checked: root.showBoundingBoxes

                                onCheckedChanged: {
                                    root.showBoundingBoxes = checked
                                    if (settings && checked !== settings.showBoundingBoxes) {
                                        settings.showBoundingBoxes = checked
                                    }
                                }
                            }

                            SettingToggle {
                                label: "Show Confidence"
                                checked: root.showConfidence

                                onCheckedChanged: {
                                    root.showConfidence = checked
                                    if (settings && checked !== settings.showConfidence) {
                                        settings.showConfidence = checked
                                    }
                                }
                            }

                            SettingToggle {
                                label: "Show Distance"
                                checked: root.showDistance

                                onCheckedChanged: {
                                    root.showDistance = checked
                                    if (settings && checked !== settings.showDistance) {
                                        settings.showDistance = checked
                                    }
                                }
                            }
                        }

                        SettingsGroup {
                            title: "Appearance"

                            SettingToggle {
                                label: "Dark Mode"
                                description: "Use dark color scheme"
                                checked: root.darkMode

                                onCheckedChanged: {
                                    if (settings && checked !== settings.darkMode) {
                                        settings.darkMode = checked
                                        root.darkMode = checked
                                    }
                                }
                            }
                        }

                        Item { height: 20 }
                    }
                }

                // Advanced Settings Tab
                ScrollView {
                    clip: true

                    ColumnLayout {
                        width: settingsDrawer.width - 40
                        spacing: 20

                        Item { height: 10 }

                        SettingsGroup {
                            title: "Detection Model"

                            SettingDropdown {
                                id: settingsModelCombo
                                label: "Model Type"
                                model: ListModel {
                                    ListElement { text: "YuNet ONNX"; modelType: 0 }
                                    ListElement { text: "ResNet10 Caffe"; modelType: 1 }
                                    ListElement { text: "MobileNet Caffe"; modelType: 2 }
                                }
                                textRole: "text"
                                currentIndex: settingsModelCombo.currentIndex

                                onCurrentIndexChanged: {
                                    if (currentIndex >= 0) {
                                        var modelType = model.get(currentIndex).modelType;
                                        root.modelSelected(modelType);
                                    }
                                }
                            }

                            SettingSlider {
                                label: "Confidence Threshold"
                                from: 0.1
                                to: 1.0
                                value: settings ? settings.confidenceThreshold : 0.5
                                stepSize: 0.05
                                suffix: "%"

                                onValueChanged: {
                                    if (settings && value !== settings.confidenceThreshold) {
                                        settings.confidenceThreshold = value
                                        root.settingsChanged({ "confidence": value })
                                    }
                                }
                            }

                            SettingSlider {
                                label: "NMS Threshold"
                                from: 0.1
                                to: 1.0
                                value: settings ? settings.nmsThreshold : 0.4
                                stepSize: 0.05
                                suffix: "%"

                                onValueChanged: {
                                    if (settings && value !== settings.nmsThreshold) {
                                        settings.nmsThreshold = value
                                        root.settingsChanged({ "nms": value })
                                    }
                                }
                            }
                        }

                        SettingsGroup {
                            title: "Processing"

                            SettingToggle {
                                id: gpuToggle
                                label: "GPU Acceleration"
                                description: "Use GPU for faster processing (requires restart)"
                                checked: settings ? settings.gpuEnabled : false

                                onCheckedChanged: {
                                    if (settings && checked !== settings.gpuEnabled) {
                                        settings.gpuEnabled = checked
                                        root.settingsChanged({ "gpu": checked })
                                    }
                                }
                            }

                            SettingToggle {
                                label: "Verbose Logging"
                                checked: settings ? settings.verboseLogging : false

                                onCheckedChanged: {
                                    if (settings && checked !== settings.verboseLogging) {
                                        settings.verboseLogging = checked
                                        root.settingsChanged({ "verbose": checked })
                                    }
                                }
                            }
                        }

                        Item { height: 20 }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: themeDividerColor
            }

            // Footer
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                color: themeSurfaceColor

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 12

                    Button {
                        text: "Reset"
                        flat: true
                        font.pixelSize: 13
                        font.family: "Segoe UI"

                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: themeTextPrimary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.hovered ? layerColor : "transparent"
                            radius: 4
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Apply"
                        font.pixelSize: 13
                        font.family: "Segoe UI"
                        font.weight: Font.DemiBold
                        Layout.preferredHeight: 36
                        Layout.preferredWidth: 100

                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.pressed ? accentPressed : parent.hovered ? accentHover : accentColor
                            radius: 4
                        }
                    }
                }
            }
        }
    }

    // Camera list model
    ListModel {
        id: cameraModel
    }

    // C++ interface functions
    function updateFrame(imageSource) {
        videoImage.source = imageSource;
    }

    function updateFaces(faceData) {
        faceOverlay.updateFaces(faceData);
    }

    function updateCameraList(cameras, currentId) {
        cameraModel.clear();
        var currentIndex = 0;
        for (var i = 0; i < cameras.length; i++) {
            cameraModel.append({
                "display": cameras[i].description,
                "deviceId": cameras[i].id
            });
            if (cameras[i].id === currentId) {
                currentIndex = i;
            }
        }
        settingsCameraCombo.currentIndex = currentIndex;
    }

    function setCurrentModel(modelType) {
        settingsModelCombo.currentIndex = modelType;
    }

    function updateAvailableDevices(devices) {
        root.availableDevices = devices;
        scanningIndicator.running = false;
    }

    // Custom Components
    component StatusPill: Rectangle {
        property string label: ""
        property string value: "0"
        property color statusColor: textSecondary

        implicitWidth: contentRow.width + 16
        implicitHeight: 28
        radius: 4
        color: layerColor

        RowLayout {
            id: contentRow
            anchors.centerIn: parent
            spacing: 6

            Label {
                text: label
                font.pixelSize: 11
                font.family: "Segoe UI"
                color: textSecondary
            }

            Rectangle {
                width: 3
                height: 3
                radius: 1.5
                color: statusColor
            }

            Label {
                text: value
                font.pixelSize: 12
                font.family: "Segoe UI"
                font.weight: Font.DemiBold
                color: statusColor

                Behavior on color {
                    ColorAnimation { duration: 200 }
                }
            }
        }
    }

    component ControlSection: ColumnLayout {
        property string title: ""

        spacing: 6

        Label {
            text: title
            font.pixelSize: 11
            font.family: "Segoe UI"
            font.weight: Font.Medium
            color: textSecondary
        }
    }

    component SettingsGroup: ColumnLayout {
        property string title: ""

        Layout.fillWidth: true
        spacing: 12

        Label {
            text: title
            font.pixelSize: 14
            font.family: "Segoe UI"
            font.weight: Font.DemiBold
            color: themeTextPrimary
            Layout.leftMargin: 20
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            color: themeDividerColor
        }
    }

    component SettingSlider: ColumnLayout {
        property string label: ""
        property real from: 0
        property real to: 100
        property real value: 50
        property real stepSize: 1
        property string suffix: ""

        Layout.fillWidth: true
        Layout.leftMargin: 20
        Layout.rightMargin: 20
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: label
                font.pixelSize: 13
                font.family: "Segoe UI"
                color: themeTextPrimary
            }

            Item { Layout.fillWidth: true }

            Label {
                text: Math.round(slider.value).toString() + (suffix ? suffix : "")
                font.pixelSize: 13
                font.family: "Segoe UI"
                font.weight: Font.DemiBold
                color: accentColor
            }
        }

        Slider {
            id: slider
            Layout.fillWidth: true
            from: parent.from
            to: parent.to
            value: parent.value
            stepSize: parent.stepSize

            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: slider.availableWidth
                height: 4
                radius: 2
                color: layerColor

                Rectangle {
                    width: slider.visualPosition * parent.width
                    height: parent.height
                    color: accentColor
                    radius: 2
                }
            }

            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: 16
                height: 16
                radius: 8
                color: accentColor
                border.color: "white"
                border.width: 2
            }

            onValueChanged: parent.value = value
        }
    }

    component SettingToggle: Rectangle {
        property string label: ""
        property string description: ""
        property alias checked: toggle.checked

        Layout.fillWidth: true
        Layout.leftMargin: 20
        Layout.rightMargin: 20
        Layout.preferredHeight: description ? 64 : 48
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 0
            anchors.rightMargin: 0
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: label
                    font.pixelSize: 13
                    font.family: "Segoe UI"
                    color: themeTextPrimary
                }

                Label {
                    text: description
                    font.pixelSize: 11
                    font.family: "Segoe UI"
                    color: themeTextSecondary
                    visible: description
                }
            }

            Switch {
                id: toggle

                indicator: Rectangle {
                    implicitWidth: 40
                    implicitHeight: 20
                    x: toggle.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: 10
                    color: toggle.checked ? accentColor : dividerColor

                    Behavior on color {
                        ColorAnimation { duration: 100 }
                    }

                    Rectangle {
                        x: toggle.checked ? parent.width - width - 2 : 2
                        y: 2
                        width: 16
                        height: 16
                        radius: 8
                        color: themeSurfaceColor

                        Behavior on x {
                            NumberAnimation { duration: 100 }
                        }
                    }
                }
            }
        }
    }

    component SettingDropdown: ColumnLayout {
        property string label: ""
        property alias model: combo.model
        property alias currentIndex: combo.currentIndex
        property alias textRole: combo.textRole

        spacing: 8
        Layout.fillWidth: true
        Layout.leftMargin: 0
        Layout.rightMargin: 0

        Text {
            text: label
            font.pixelSize: 13
            font.family: "Segoe UI"
            color: root.darkMode ? "white" : textPrimary
        }

        ComboBox {
            id: combo
            Layout.fillWidth: true
            font.pixelSize: 13
            font.family: "Segoe UI"

            background: Rectangle {
                color: parent.pressed ? layerColor : (root.darkMode ? "#3A3A3A" : "white")
                border.color: parent.hovered ? accentColor : themeDividerColor
                border.width: 1
                radius: 4
            }

            contentItem: Text {
                leftPadding: 12
                rightPadding: 12
                text: combo.displayText
                font: combo.font
                color: root.darkMode ? "white" : textPrimary
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }

    // Window close handling
    onClosing: (close) => {
        if (backend) {
            root.quitRequested();
        }
        close.accepted = true;
    }

    // Window visibility changed handler
    onVisibilityChanged: (visibility) => {
        if (visibility === ApplicationWindow.Minimized || visibility === ApplicationWindow.Hidden) {
            // Optionally pause camera when minimized
            // Uncomment to auto-hide preview when minimized:
            // root.cameraPreviewVisible = false
        }
    }
}
