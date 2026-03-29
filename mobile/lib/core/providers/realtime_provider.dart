import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/sensor_data.dart';
import '../services/mqtt_service.dart';
import 'monitoring_provider.dart';

// Real-time connection state
class RealtimeState {
  final AppMqttState connectionState;
  final String? currentDeviceId;
  final DateTime? lastMessageAt;
  final String? error;

  const RealtimeState({
    this.connectionState = AppMqttState.disconnected,
    this.currentDeviceId,
    this.lastMessageAt,
    this.error,
  });

  RealtimeState copyWith({
    AppMqttState? connectionState,
    String? currentDeviceId,
    DateTime? lastMessageAt,
    String? error,
  }) {
    return RealtimeState(
      connectionState: connectionState ?? this.connectionState,
      currentDeviceId: currentDeviceId ?? this.currentDeviceId,
      lastMessageAt: lastMessageAt ?? this.lastMessageAt,
      error: error,
    );
  }

  bool get isConnected => connectionState == AppMqttState.connected;
}

// Real-time notifier
class RealtimeNotifier extends StateNotifier<RealtimeState> {
  final Ref _ref;
  final MqttService _mqttService;
  StreamSubscription? _connectionSubscription;
  StreamSubscription? _messageSubscription;

  RealtimeNotifier(this._ref, this._mqttService)
    : super(const RealtimeState()) {
    _setupListeners();
  }

  void _setupListeners() {
    _connectionSubscription = _mqttService.connectionState.listen((mqttState) {
      state = state.copyWith(connectionState: mqttState);
    });

    _messageSubscription = _mqttService.messages.listen(_handleMessage);
  }

  void _handleMessage(MqttMessage message) {
    state = state.copyWith(lastMessageAt: DateTime.now());

    final topic = message.topic;
    final payload = message.jsonPayload;
    if (payload == null) return;

    // Handle telemetry updates
    if (topic.contains('/telemetry')) {
      _handleTelemetry(payload);
    }
    // Handle shadow updates
    else if (topic.contains('/shadow')) {
      _handleShadowUpdate(payload);
    }
    // Handle alerts
    else if (topic.contains('/alerts')) {
      _handleAlert(payload);
    }
  }

  void _handleTelemetry(Map<String, dynamic> payload) {
    try {
      final sensorData = SensorData.fromJson(payload);
      _ref.read(sensorDataProvider.notifier).updateFromMqtt(sensorData);
    } catch (e) {
      // Log error but don't crash
    }
  }

  void _handleShadowUpdate(Map<String, dynamic> payload) {
    // Handle device shadow state updates
    // This could update device online status, config changes, etc.
  }

  void _handleAlert(Map<String, dynamic> payload) {
    try {
      final alert = DeviceAlert.fromJson(payload);
      _ref.read(alertsProvider.notifier).addAlert(alert);
    } catch (e) {
      // Log error but don't crash
    }
  }

  Future<bool> connect() async {
    state = state.copyWith(
      connectionState: AppMqttState.connecting,
      error: null,
    );

    try {
      final success = await _mqttService.connect();
      if (!success) {
        state = state.copyWith(
          connectionState: AppMqttState.error,
          error: 'Failed to connect to MQTT broker',
        );
      }
      return success;
    } catch (e) {
      state = state.copyWith(
        connectionState: AppMqttState.error,
        error: e.toString(),
      );
      return false;
    }
  }

  void subscribeToDevice(String deviceId) {
    if (!state.isConnected) return;

    // Unsubscribe from previous device
    if (state.currentDeviceId != null && state.currentDeviceId != deviceId) {
      _mqttService.unsubscribeFromDevice(state.currentDeviceId!);
    }

    _mqttService.subscribeToDevice(deviceId);
    state = state.copyWith(currentDeviceId: deviceId);
  }

  void unsubscribeFromDevice(String deviceId) {
    _mqttService.unsubscribeFromDevice(deviceId);
    if (state.currentDeviceId == deviceId) {
      state = state.copyWith(currentDeviceId: null);
    }
  }

  void sendFeedCommand(String deviceId, double amount) {
    _mqttService.sendCommand(deviceId, 'feed', {'amount': amount});
  }

  void sendEmergencyStop(String deviceId) {
    _mqttService.sendCommand(deviceId, 'emergency_stop', {});
  }

  void requestVideoCapture(String deviceId) {
    _mqttService.sendCommand(deviceId, 'capture_video', {});
  }

  void disconnect() {
    if (state.currentDeviceId != null) {
      _mqttService.unsubscribeFromDevice(state.currentDeviceId!);
    }
    _mqttService.disconnect();
    state = state.copyWith(currentDeviceId: null);
  }

  @override
  void dispose() {
    _connectionSubscription?.cancel();
    _messageSubscription?.cancel();
    disconnect();
    super.dispose();
  }
}

// Providers
final mqttServiceProvider = Provider<MqttService>((ref) {
  return MqttService();
});

final realtimeProvider = StateNotifierProvider<RealtimeNotifier, RealtimeState>(
  (ref) {
    final mqttService = ref.watch(mqttServiceProvider);
    return RealtimeNotifier(ref, mqttService);
  },
);

// Convenience providers
final isRealtimeConnectedProvider = Provider<bool>((ref) {
  return ref.watch(realtimeProvider).isConnected;
});

final realtimeConnectionStateProvider = Provider<AppMqttState>((ref) {
  return ref.watch(realtimeProvider).connectionState;
});
