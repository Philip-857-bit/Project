import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/device.dart';
import '../services/api_service.dart';
import 'auth_provider.dart';

// Device List State
class DeviceListState {
  final List<Device> devices;
  final bool isLoading;
  final String? error;

  const DeviceListState({
    this.devices = const [],
    this.isLoading = false,
    this.error,
  });

  DeviceListState copyWith({
    List<Device>? devices,
    bool? isLoading,
    String? error,
  }) {
    return DeviceListState(
      devices: devices ?? this.devices,
      isLoading: isLoading ?? this.isLoading,
      error: error,
    );
  }
}

// Device List Notifier
class DeviceListNotifier extends StateNotifier<DeviceListState> {
  final ApiService _apiService;

  DeviceListNotifier(this._apiService) : super(const DeviceListState());

  Future<void> loadDevices() async {
    state = state.copyWith(isLoading: true, error: null);

    try {
      final response = await _apiService.getDevices();
      if (response.statusCode == 200) {
        final List<dynamic> data = response.data['devices'] ?? response.data ?? [];
        final devices = data.map((json) => Device.fromJson(json)).toList();
        state = state.copyWith(devices: devices, isLoading: false);
      } else {
        state = state.copyWith(isLoading: false, error: 'Failed to load devices');
      }
    } catch (e) {
      state = state.copyWith(isLoading: false, error: e.toString());
    }
  }

  Future<bool> bindDevice(String bindingCode) async {
    try {
      final response = await _apiService.bindDevice(bindingCode);
      if (response.statusCode == 200 || response.statusCode == 201) {
        await loadDevices();
        return true;
      }
      return false;
    } catch (e) {
      return false;
    }
  }

  Future<bool> unbindDevice(String deviceId) async {
    try {
      final response = await _apiService.unbindDevice(deviceId);
      if (response.statusCode == 200) {
        state = state.copyWith(
          devices: state.devices.where((d) => d.id != deviceId).toList(),
        );
        return true;
      }
      return false;
    } catch (e) {
      return false;
    }
  }
}

// Selected Device State
class SelectedDeviceState {
  final Device? device;
  final bool isLoading;
  final String? error;

  const SelectedDeviceState({
    this.device,
    this.isLoading = false,
    this.error,
  });

  SelectedDeviceState copyWith({
    Device? device,
    bool? isLoading,
    String? error,
  }) {
    return SelectedDeviceState(
      device: device ?? this.device,
      isLoading: isLoading ?? this.isLoading,
      error: error,
    );
  }
}

// Selected Device Notifier
class SelectedDeviceNotifier extends StateNotifier<SelectedDeviceState> {
  final ApiService _apiService;

  SelectedDeviceNotifier(this._apiService) : super(const SelectedDeviceState());

  Future<void> loadDevice(String deviceId) async {
    state = state.copyWith(isLoading: true, error: null);

    try {
      final response = await _apiService.getDevice(deviceId);
      if (response.statusCode == 200) {
        final device = Device.fromJson(response.data);
        state = state.copyWith(device: device, isLoading: false);
      } else {
        state = state.copyWith(isLoading: false, error: 'Failed to load device');
      }
    } catch (e) {
      state = state.copyWith(isLoading: false, error: e.toString());
    }
  }

  void selectDevice(Device device) {
    state = state.copyWith(device: device);
  }

  void clearSelection() {
    state = const SelectedDeviceState();
  }
}

// Providers
final deviceListProvider = StateNotifierProvider<DeviceListNotifier, DeviceListState>((ref) {
  final apiService = ref.watch(apiServiceProvider);
  return DeviceListNotifier(apiService);
});

final selectedDeviceProvider = StateNotifierProvider<SelectedDeviceNotifier, SelectedDeviceState>((ref) {
  final apiService = ref.watch(apiServiceProvider);
  return SelectedDeviceNotifier(apiService);
});

// Convenience providers
final devicesProvider = Provider<List<Device>>((ref) {
  return ref.watch(deviceListProvider).devices;
});

final onlineDevicesProvider = Provider<List<Device>>((ref) {
  return ref.watch(devicesProvider).where((d) => d.isOnline).toList();
});

final deviceByIdProvider = Provider.family<Device?, String>((ref, deviceId) {
  final devices = ref.watch(devicesProvider);
  try {
    return devices.firstWhere((d) => d.id == deviceId);
  } catch (_) {
    return null;
  }
});
