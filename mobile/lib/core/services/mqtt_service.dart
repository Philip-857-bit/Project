import 'dart:async';
import 'dart:convert';

import 'package:logger/logger.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

import '../config/env_config.dart';
import 'storage_service.dart';

enum AppMqttState { disconnected, connecting, connected, error }

class MqttService {
  static final MqttService _instance = MqttService._internal();
  factory MqttService() => _instance;
  MqttService._internal();

  MqttServerClient? _client;
  final Logger _logger = Logger();
  
  final _connectionStateController = StreamController<AppMqttState>.broadcast();
  Stream<AppMqttState> get connectionState => _connectionStateController.stream;
  
  final _messageController = StreamController<MqttMessage>.broadcast();
  Stream<MqttMessage> get messages => _messageController.stream;

  AppMqttState _currentState = AppMqttState.disconnected;
  AppMqttState get currentState => _currentState;

  bool get isConnected {
    final status = _client?.connectionStatus;
    return status != null && status.state == MqttConnectionState.connected;
  }

  Future<bool> connect() async {
    if (_client != null && isConnected) {
      return true;
    }

    _updateState(AppMqttState.connecting);

    // Use stored values if available, otherwise fall back to env config
    final storedHost = StorageService.getMqttHost();
    final storedPort = StorageService.getMqttPort();
    
    // Only use stored values if they differ from defaults (user explicitly set them)
    final host = storedHost.isNotEmpty && storedHost != 'mqtt.smartfishfeeder.com' 
        ? storedHost 
        : EnvConfig.mqttHost;
    final port = storedPort != 8883 ? storedPort : EnvConfig.mqttPort;
    final clientId = 'mobile_${StorageService.getUserId() ?? DateTime.now().millisecondsSinceEpoch}';

    _client = MqttServerClient.withPort(host, clientId, port);
    _client!.logging(on: false);
    _client!.keepAlivePeriod = 60;
    _client!.autoReconnect = true;
    _client!.resubscribeOnAutoReconnect = true;
    _client!.onConnected = _onConnected;
    _client!.onDisconnected = _onDisconnected;
    _client!.onAutoReconnect = _onAutoReconnect;
    _client!.onAutoReconnected = _onAutoReconnected;

    try {
      final token = await StorageService.getAccessToken();
      final connMessage = MqttConnectMessage()
          .withClientIdentifier(clientId)
          .authenticateAs('jwt', token ?? '')
          .withWillQos(MqttQos.atLeastOnce)
          .startClean();
      
      _client!.connectionMessage = connMessage;
      
      await _client!.connect();
      
      if (isConnected) {
        _setupMessageListener();
        return true;
      }
    } catch (e) {
      _logger.e('MQTT connection failed: $e');
      _updateState(AppMqttState.error);
    }
    
    return false;
  }

  void _setupMessageListener() {
    _client!.updates?.listen((messages) {
      for (final message in messages) {
        final pubMsg = message.payload as MqttPublishMessage;
        final payloadString = MqttPublishPayload.bytesToStringAsString(pubMsg.payload.message);
        
        _logger.d('MQTT Message: ${message.topic} => $payloadString');
        
        _messageController.add(MqttMessage(
          topic: message.topic,
          payload: payloadString,
        ));
      }
    });
  }

  void _onConnected() {
    _logger.i('MQTT Connected');
    _updateState(AppMqttState.connected);
  }

  void _onDisconnected() {
    _logger.w('MQTT Disconnected');
    _updateState(AppMqttState.disconnected);
  }

  void _onAutoReconnect() {
    _logger.i('MQTT Auto-reconnecting...');
    _updateState(AppMqttState.connecting);
  }

  void _onAutoReconnected() {
    _logger.i('MQTT Auto-reconnected');
    _updateState(AppMqttState.connected);
  }

  void _updateState(AppMqttState state) {
    _currentState = state;
    _connectionStateController.add(state);
  }

  void subscribe(String topic, {MqttQos qos = MqttQos.atLeastOnce}) {
    if (!isConnected) {
      _logger.w('Cannot subscribe: not connected');
      return;
    }
    _client!.subscribe(topic, qos);
    _logger.d('Subscribed to: $topic');
  }

  void unsubscribe(String topic) {
    _client?.unsubscribe(topic);
    _logger.d('Unsubscribed from: $topic');
  }

  void publish(String topic, String message, {MqttQos qos = MqttQos.atLeastOnce, bool retain = false}) {
    if (!isConnected) {
      _logger.w('Cannot publish: not connected');
      return;
    }
    
    final builder = MqttClientPayloadBuilder();
    builder.addString(message);
    _client!.publishMessage(topic, qos, builder.payload!, retain: retain);
    _logger.d('Published to $topic: $message');
  }

  void publishJson(String topic, Map<String, dynamic> data, {MqttQos qos = MqttQos.atLeastOnce}) {
    publish(topic, jsonEncode(data), qos: qos);
  }

  void disconnect() {
    _client?.disconnect();
    _updateState(AppMqttState.disconnected);
  }

  void dispose() {
    disconnect();
    _connectionStateController.close();
    _messageController.close();
  }

  String deviceTelemetryTopic(String deviceId) => 'devices/$deviceId/telemetry';
  String deviceCommandTopic(String deviceId) => 'devices/$deviceId/commands';
  String deviceShadowTopic(String deviceId) => 'devices/$deviceId/shadow';
  String deviceAlertsTopic(String deviceId) => 'devices/$deviceId/alerts';

  void subscribeToDevice(String deviceId) {
    subscribe(deviceTelemetryTopic(deviceId));
    subscribe(deviceShadowTopic(deviceId));
    subscribe(deviceAlertsTopic(deviceId));
  }

  void unsubscribeFromDevice(String deviceId) {
    unsubscribe(deviceTelemetryTopic(deviceId));
    unsubscribe(deviceShadowTopic(deviceId));
    unsubscribe(deviceAlertsTopic(deviceId));
  }

  void sendCommand(String deviceId, String command, Map<String, dynamic> payload) {
    publishJson(deviceCommandTopic(deviceId), {
      'command': command,
      'payload': payload,
      'timestamp': DateTime.now().toIso8601String(),
    });
  }
}

class MqttMessage {
  final String topic;
  final String payload;

  MqttMessage({required this.topic, required this.payload});

  Map<String, dynamic>? get jsonPayload {
    try {
      return jsonDecode(payload);
    } catch (_) {
      return null;
    }
  }
}
