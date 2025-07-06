package com.example.hydropreserve;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import android.content.SharedPreferences;
import android.graphics.Color;
import android.os.Bundle;
import android.text.InputType;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class MainActivity extends AppCompatActivity implements HiveMqttClient.MqttCallback {
    private static final String TAG = "MainActivity";

    private TextView tankLevelTextView;
    private ProgressBar tankLevelProgressBar;
    private TextView pumpStatusTextView;
    private TextView pumpStatusIndicator;
    private Button overrideButton;
    private TextView connectionStatusTextView;
    private Button connectButton;
    private TextView lastUpdatedTextView;

    private HiveMqttClient mqttClient;
    private SharedPreferences sharedPreferences;
    private static final String PREFS_NAME = "HydroPreservePrefs";
    private static final String BROKER_IP_KEY = "broker_ip";
    private static final String DEFAULT_BROKER_IP = "192.168.43.1"; // Default hotspot IP

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Initialize UI components
        tankLevelTextView = findViewById(R.id.tankLevelTextView);
        tankLevelProgressBar = findViewById(R.id.tankLevelProgressBar);
        pumpStatusTextView = findViewById(R.id.pumpStatusTextView);
        pumpStatusIndicator = findViewById(R.id.pumpStatusIndicator);
        overrideButton = findViewById(R.id.overrideButton);
        connectionStatusTextView = findViewById(R.id.connectionStatusTextView);
        connectButton = findViewById(R.id.connectButton);
        lastUpdatedTextView = findViewById(R.id.lastUpdatedTextView);

        // Initialize SharedPreferences
        sharedPreferences = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);

        // Set up button click listeners
        connectButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mqttClient != null && mqttClient.isConnected()) {
                    mqttClient.disconnect();
                    connectButton.setText("Connect to MQTT Broker");
                } else {
                    showBrokerIpDialog();
                }
            }
        });

        overrideButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mqttClient != null && mqttClient.isConnected()) {
                    // Send command to turn off the pump
                    mqttClient.publishPumpControl(true);
                    Toast.makeText(MainActivity.this, "Emergency stop command sent", Toast.LENGTH_SHORT).show();
                } else {
                    Toast.makeText(MainActivity.this, "Not connected to broker", Toast.LENGTH_SHORT).show();
                }
            }
        });
    }

    private void showBrokerIpDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Broker IP Address");

        // Set up the input
        final EditText input = new EditText(this);
        input.setInputType(InputType.TYPE_CLASS_TEXT);
        input.setText(sharedPreferences.getString(BROKER_IP_KEY, DEFAULT_BROKER_IP));
        builder.setView(input);

        // Set up the buttons
        builder.setPositiveButton("Connect", (dialog, which) -> {
            String brokerIp = input.getText().toString().trim();
            if (!brokerIp.isEmpty()) {
                // Save the IP address
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putString(BROKER_IP_KEY, brokerIp);
                editor.apply();

                // Show connecting status
                connectionStatusTextView.setText("Connection Status: Connecting...");

                try {
                    // Create and connect MQTT client
                    Log.d(TAG, "Creating MQTT client with broker IP: " + brokerIp);
                    if (mqttClient != null) {
                        mqttClient.disconnect();
                        mqttClient.cleanup();
                    }

                    mqttClient = new HiveMqttClient(getApplicationContext(), brokerIp, MainActivity.this);
                    mqttClient.connect();
                } catch (Exception e) {
                    Log.e(TAG, "Error creating/connecting MQTT client", e);
                    Toast.makeText(MainActivity.this, "Error: " + e.getMessage(), Toast.LENGTH_LONG).show();
                    connectionStatusTextView.setText("Connection Status: Failed");
                }
            }
        });
        builder.setNegativeButton("Cancel", (dialog, which) -> dialog.cancel());

        builder.show();
    }

    @Override
    protected void onDestroy() {
        if (mqttClient != null) {
            mqttClient.disconnect();
            mqttClient.cleanup();
        }
        super.onDestroy();
    }

    // MQTT Callback methods
    @Override
    public void onConnectionChanged(boolean connected) {
        if (connected) {
            connectionStatusTextView.setText("Connection Status: Connected");
            connectButton.setText("Disconnect");
        } else {
            connectionStatusTextView.setText("Connection Status: Disconnected");
            connectButton.setText("Connect to MQTT Broker");
        }
    }

    @Override
    public void onTankLevelReceived(int level) {
        tankLevelTextView.setText(level + "%");
        tankLevelProgressBar.setProgress(level);
        updateLastUpdatedTime();
    }

    @Override
    public void onPumpStatusReceived(boolean isOn) {
        if (isOn) {
            pumpStatusTextView.setText("ON");
            pumpStatusIndicator.setText("Running");
            pumpStatusIndicator.setBackgroundColor(Color.parseColor("#4CAF50")); // Green
            pumpStatusIndicator.setTextColor(Color.WHITE); // White text on green background
        } else {
            pumpStatusTextView.setText("OFF");
            pumpStatusIndicator.setText("Stopped");
            pumpStatusIndicator.setBackgroundColor(Color.parseColor("#F44336")); // Red
            pumpStatusIndicator.setTextColor(Color.WHITE); // White text on red background
        }
        updateLastUpdatedTime();
    }

    @Override
    public void onError(String errorMessage) {
        Log.e(TAG, "MQTT Error: " + errorMessage);
        Toast.makeText(MainActivity.this, errorMessage, Toast.LENGTH_SHORT).show();
    }

    private void updateLastUpdatedTime() {
        SimpleDateFormat sdf = new SimpleDateFormat("HH:mm:ss", Locale.getDefault());
        String currentTime = sdf.format(new Date());
        lastUpdatedTextView.setText("Last Updated: " + currentTime);
    }
}
