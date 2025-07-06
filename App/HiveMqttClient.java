package com.example.hydropreserve;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.hivemq.client.mqtt.MqttClient;
import com.hivemq.client.mqtt.MqttGlobalPublishFilter;
import com.hivemq.client.mqtt.datatypes.MqttQos;
import com.hivemq.client.mqtt.mqtt3.Mqtt3BlockingClient;
import com.hivemq.client.mqtt.mqtt3.Mqtt3Client;
import com.hivemq.client.mqtt.mqtt3.message.connect.connack.Mqtt3ConnAck;
import com.hivemq.client.mqtt.mqtt3.message.publish.Mqtt3Publish;
import com.hivemq.client.mqtt.mqtt3.message.subscribe.suback.Mqtt3SubAck;

import java.nio.charset.StandardCharsets;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * MQTT Client using HiveMQ library
 */
public class HiveMqttClient {
    private static final String TAG = "HiveMqttClient";

    private final String serverUri;
    private final String clientId;
    private final String publishTopic;
    private final String tankLevelTopic;
    private final String pumpStatusTopic;

    private Mqtt3Client mqttClient;
    private final Context context;
    private final MqttCallback mqttCallback;
    private final Handler mainHandler;
    private boolean isConnected = false;
    private ExecutorService executor;

    public interface MqttCallback {
        void onConnectionChanged(boolean connected);
        void onTankLevelReceived(int level);
        void onPumpStatusReceived(boolean isOn);
        void onError(String errorMessage);
    }

    public HiveMqttClient(Context context, String brokerIp, MqttCallback callback) {
        this.context = context;
        this.mqttCallback = callback;
        this.mainHandler = new Handler(Looper.getMainLooper());
        this.executor = Executors.newSingleThreadExecutor();

        this.clientId = "HydroPreserveApp_" + UUID.randomUUID().toString().substring(0, 8);
        this.serverUri = brokerIp;
        this.publishTopic = "hydropreserve/control";
        this.tankLevelTopic = "hydropreserve/tank_level";
        this.pumpStatusTopic = "hydropreserve/pump_status";

        try {
            Log.d(TAG, "Creating MQTT client for broker: " + brokerIp);

            // Build the client
            mqttClient = MqttClient.builder()
                    .useMqttVersion3()
                    .identifier(clientId)
                    .serverHost(serverUri)
                    .serverPort(1883)
                    .buildAsync();

            Log.d(TAG, "MQTT client created successfully");
        } catch (Exception e) {
            Log.e(TAG, "Error creating MQTT client", e);
            runOnMainThread(() -> mqttCallback.onError("Error creating MQTT client: " + e.getMessage()));
        }
    }

    public void connect() {
        if (mqttClient == null) {
            runOnMainThread(() -> mqttCallback.onError("MQTT client is null, cannot connect"));
            return;
        }

        executor.execute(() -> {
            try {
                Log.d(TAG, "Attempting to connect to broker: " + serverUri);

                // Connect with a timeout
                CompletableFuture<Mqtt3ConnAck> connectFuture = mqttClient.toAsync()
                        .connectWith()
                        .cleanSession(true)
                        .keepAlive(30)
                        .send();

                connectFuture.whenComplete((connAck, throwable) -> {
                    if (throwable != null) {
                        Log.e(TAG, "Connection failed", throwable);
                        isConnected = false;
                        runOnMainThread(() -> {
                            mqttCallback.onConnectionChanged(false);
                            mqttCallback.onError("Connection failed: " + throwable.getMessage());
                        });
                    } else {
                        Log.d(TAG, "Connected successfully");
                        isConnected = true;
                        runOnMainThread(() -> mqttCallback.onConnectionChanged(true));

                        // Subscribe to topics
                        subscribeToTopics();
                    }
                });
            } catch (Exception e) {
                Log.e(TAG, "Error connecting to MQTT broker", e);
                isConnected = false;
                runOnMainThread(() -> {
                    mqttCallback.onConnectionChanged(false);
                    mqttCallback.onError("Connection error: " + e.getMessage());
                });
            }
        });
    }

    private void subscribeToTopics() {
        // Subscribe to all messages
        mqttClient.toAsync().subscribeWith()
                .topicFilter(tankLevelTopic)
                .qos(MqttQos.AT_MOST_ONCE)
                .send()
                .whenComplete((subAck, throwable) -> {
                    if (throwable != null) {
                        Log.e(TAG, "Failed to subscribe to tank level topic", throwable);
                        runOnMainThread(() -> mqttCallback.onError("Subscription error: " + throwable.getMessage()));
                    } else {
                        Log.d(TAG, "Subscribed to tank level topic");
                    }
                });

        mqttClient.toAsync().subscribeWith()
                .topicFilter(pumpStatusTopic)
                .qos(MqttQos.AT_MOST_ONCE)
                .send()
                .whenComplete((subAck, throwable) -> {
                    if (throwable != null) {
                        Log.e(TAG, "Failed to subscribe to pump status topic", throwable);
                        runOnMainThread(() -> mqttCallback.onError("Subscription error: " + throwable.getMessage()));
                    } else {
                        Log.d(TAG, "Subscribed to pump status topic");
                    }
                });

        // Set up a global callback for all received messages
        mqttClient.toAsync().publishes(MqttGlobalPublishFilter.ALL, this::handleMessage);
    }

    private void handleMessage(Mqtt3Publish publish) {
        String topic = publish.getTopic().toString();
        String payload = new String(publish.getPayloadAsBytes(), StandardCharsets.UTF_8);

        Log.d(TAG, "Message received: " + topic + " -> " + payload);

        if (topic.equals(tankLevelTopic)) {
            try {
                final int level = Integer.parseInt(payload.trim());
                runOnMainThread(() -> mqttCallback.onTankLevelReceived(level));
            } catch (NumberFormatException e) {
                Log.e(TAG, "Invalid tank level format: " + payload, e);
                runOnMainThread(() -> mqttCallback.onError("Invalid tank level format: " + payload));
            }
        } else if (topic.equals(pumpStatusTopic)) {
            final boolean isOn = payload.trim().equalsIgnoreCase("ON");
            runOnMainThread(() -> mqttCallback.onPumpStatusReceived(isOn));
        }
    }

    public void publishPumpControl(boolean turnOff) {
        if (!isConnected || mqttClient == null) {
            runOnMainThread(() -> mqttCallback.onError("Cannot publish: not connected"));
            return;
        }

        String message = turnOff ? "OFF" : "ON";
        executor.execute(() -> {
            try {
                mqttClient.toAsync().publishWith()
                        .topic(publishTopic)
                        .payload(message.getBytes(StandardCharsets.UTF_8))
                        .qos(MqttQos.AT_MOST_ONCE)
                        .retain(false)
                        .send()
                        .whenComplete((publishResult, throwable) -> {
                            if (throwable != null) {
                                Log.e(TAG, "Failed to publish message", throwable);
                                runOnMainThread(() -> mqttCallback.onError("Publish failed: " + throwable.getMessage()));
                            } else {
                                Log.d(TAG, "Message published successfully");
                            }
                        });
            } catch (Exception e) {
                Log.e(TAG, "Error publishing message", e);
                runOnMainThread(() -> mqttCallback.onError("Publish error: " + e.getMessage()));
            }
        });
    }

    public void disconnect() {
        if (mqttClient != null) {
            executor.execute(() -> {
                try {
                    mqttClient.toAsync().disconnect();
                    isConnected = false;
                    runOnMainThread(() -> mqttCallback.onConnectionChanged(false));
                    Log.d(TAG, "Disconnected from broker");
                } catch (Exception e) {
                    Log.e(TAG, "Error disconnecting", e);
                    runOnMainThread(() -> mqttCallback.onError("Disconnect error: " + e.getMessage()));
                }
            });
        }
    }

    public boolean isConnected() {
        return isConnected;
    }

    private void runOnMainThread(Runnable runnable) {
        if (mainHandler != null) {
            mainHandler.post(runnable);
        }
    }

    // Clean up resources
    public void cleanup() {
        disconnect();
        executor.shutdown();
    }
}
