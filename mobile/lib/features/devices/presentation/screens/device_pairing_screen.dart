import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import 'package:mobile_scanner/mobile_scanner.dart';

import '../../../../core/services/ble_service.dart';
import '../../../../core/providers/device_provider.dart';

class DevicePairingScreen extends ConsumerStatefulWidget {
  const DevicePairingScreen({super.key});

  @override
  ConsumerState<DevicePairingScreen> createState() => _DevicePairingScreenState();
}

class _DevicePairingScreenState extends ConsumerState<DevicePairingScreen> {
  final BleService _bleService = BleService();
  int _currentStep = 0;
  bool _isScanning = false;
  bool _isConnecting = false;
  bool _isProvisioning = false;
  String? _selectedDeviceId;
  String? _scannedSerialNumber;
  String _networkType = 'cellular';
  final _apnController = TextEditingController(text: 'internet');
  final _ssidController = TextEditingController();
  final _passwordController = TextEditingController();
  final _deviceNameController = TextEditingController(text: 'My Fish Feeder');
  List<BleDevice> _discoveredDevices = [];
  StreamSubscription? _scanSubscription;
  String? _bindingCode;
  String? _errorMessage;

  @override
  void initState() {
    super.initState();
    _checkBluetooth();
    _setupBleListener();
  }

  @override
  void dispose() {
    _scanSubscription?.cancel();
    _apnController.dispose();
    _ssidController.dispose();
    _passwordController.dispose();
    _deviceNameController.dispose();
    super.dispose();
  }

  void _setupBleListener() {
    _scanSubscription = _bleService.discoveredDevices.listen((devices) {
      setState(() => _discoveredDevices = devices);
    });
  }

  Future<void> _checkBluetooth() async {
    final isAvailable = await _bleService.isBluetoothAvailable();
    if (!isAvailable && mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Bluetooth is not available on this device')),
      );
    }
  }

  Future<void> _startScan() async {
    setState(() {
      _isScanning = true;
      _discoveredDevices = [];
      _errorMessage = null;
    });
    
    try {
      await _bleService.startScan();
    } catch (e) {
      setState(() => _errorMessage = 'Scan failed: $e');
    } finally {
      if (mounted) {
        setState(() => _isScanning = false);
      }
    }
  }

  Future<void> _connectToDevice(BleDevice device) async {
    setState(() {
      _isConnecting = true;
      _errorMessage = null;
    });

    try {
      final success = await _bleService.connectToDevice(device.id);
      if (success) {
        setState(() {
          _selectedDeviceId = device.id;
          _currentStep = 2;
        });
      } else {
        setState(() => _errorMessage = 'Failed to connect to device');
      }
    } catch (e) {
      setState(() => _errorMessage = 'Connection error: $e');
    } finally {
      if (mounted) {
        setState(() => _isConnecting = false);
      }
    }
  }

  Future<void> _provisionDevice() async {
    setState(() {
      _isProvisioning = true;
      _errorMessage = null;
    });

    try {
      bool success;
      if (_networkType == 'cellular') {
        success = await _bleService.provisionCellular(_apnController.text);
      } else {
        success = await _bleService.provisionWifi(
          _ssidController.text,
          _passwordController.text,
        );
      }

      if (success) {
        // Get binding code from device
        _bindingCode = await _bleService.getBindingCode();
        
        // Bind device to user account
        if (_bindingCode != null) {
          final bindSuccess = await ref.read(deviceListProvider.notifier).bindDevice(_bindingCode!);
          if (bindSuccess) {
            setState(() => _currentStep = 3);
          } else {
            setState(() => _errorMessage = 'Failed to bind device to your account');
          }
        }
      } else {
        setState(() => _errorMessage = 'Failed to provision device');
      }
    } catch (e) {
      setState(() => _errorMessage = 'Provisioning error: $e');
    } finally {
      if (mounted) {
        setState(() => _isProvisioning = false);
      }
    }
  }

  void _showQrScanner() {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      builder: (ctx) => SizedBox(
        height: MediaQuery.of(context).size.height * 0.7,
        child: Column(
          children: [
            AppBar(
              title: const Text('Scan QR Code'),
              leading: IconButton(
                icon: const Icon(Icons.close),
                onPressed: () => Navigator.pop(ctx),
              ),
            ),
            Expanded(
              child: MobileScanner(
                onDetect: (capture) {
                  final barcodes = capture.barcodes;
                  if (barcodes.isNotEmpty) {
                    final code = barcodes.first.rawValue;
                    if (code != null && code.startsWith('SFF-')) {
                      Navigator.pop(ctx);
                      setState(() {
                        _scannedSerialNumber = code;
                        _currentStep = 1;
                      });
                      _startScan();
                    }
                  }
                },
              ),
            ),
            Padding(
              padding: const EdgeInsets.all(16),
              child: Text(
                'Point camera at the QR code on your device',
                style: TextStyle(color: Colors.grey[600]),
                textAlign: TextAlign.center,
              ),
            ),
          ],
        ),
      ),
    );
  }

  void _showManualEntry() {
    final controller = TextEditingController();
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Enter Serial Number'),
        content: TextField(
          controller: controller,
          decoration: const InputDecoration(
            labelText: 'Serial Number',
            hintText: 'SFF-XXX-XXXXXX',
            border: OutlineInputBorder(),
          ),
          textCapitalization: TextCapitalization.characters,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () {
              if (controller.text.isNotEmpty) {
                Navigator.pop(ctx);
                setState(() {
                  _scannedSerialNumber = controller.text;
                  _currentStep = 1;
                });
                _startScan();
              }
            },
            child: const Text('Continue'),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Add Device'),
      ),
      body: Stepper(
        currentStep: _currentStep,
        onStepContinue: _handleStepContinue,
        onStepCancel: _handleStepCancel,
        controlsBuilder: (context, details) {
          return Padding(
            padding: const EdgeInsets.only(top: 16),
            child: Row(
              children: [
                if (_currentStep < 3)
                  FilledButton(
                    onPressed: _canContinue() ? details.onStepContinue : null,
                    child: _getButtonChild(),
                  ),
                if (_currentStep == 3)
                  FilledButton(
                    onPressed: () => context.go('/devices'),
                    child: const Text('Done'),
                  ),
                const SizedBox(width: 12),
                if (_currentStep > 0 && _currentStep < 3)
                  TextButton(
                    onPressed: details.onStepCancel,
                    child: const Text('Back'),
                  ),
              ],
            ),
          );
        },
        steps: [
          // Step 0: Scan QR or Enter Serial
          Step(
            title: const Text('Identify Device'),
            content: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'Scan the QR code on your device or enter the serial number manually.',
                  style: TextStyle(color: Colors.grey),
                ),
                const SizedBox(height: 24),
                Row(
                  children: [
                    Expanded(
                      child: _ActionCard(
                        icon: Icons.qr_code_scanner,
                        title: 'Scan QR Code',
                        subtitle: 'Quick and easy',
                        onTap: _showQrScanner,
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: _ActionCard(
                        icon: Icons.keyboard,
                        title: 'Enter Manually',
                        subtitle: 'Type serial number',
                        onTap: _showManualEntry,
                      ),
                    ),
                  ],
                ),
                if (_scannedSerialNumber != null) ...[
                  const SizedBox(height: 16),
                  Card(
                    color: Colors.green.shade50,
                    child: ListTile(
                      leading: const Icon(Icons.check_circle, color: Colors.green),
                      title: const Text('Device Found'),
                      subtitle: Text(_scannedSerialNumber!),
                    ),
                  ),
                ],
              ],
            ),
            isActive: _currentStep >= 0,
          ),

          // Step 1: Find and Connect via BLE
          Step(
            title: const Text('Connect Device'),
            content: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                if (_scannedSerialNumber != null)
                  Text('Looking for: $_scannedSerialNumber'),
                const SizedBox(height: 16),
                
                if (_isScanning)
                  const Center(
                    child: Column(
                      children: [
                        CircularProgressIndicator(),
                        SizedBox(height: 16),
                        Text('Scanning for nearby devices...'),
                      ],
                    ),
                  )
                else if (_isConnecting)
                  const Center(
                    child: Column(
                      children: [
                        CircularProgressIndicator(),
                        SizedBox(height: 16),
                        Text('Connecting to device...'),
                      ],
                    ),
                  )
                else ...[
                  if (_discoveredDevices.isEmpty)
                    Card(
                      child: Padding(
                        padding: const EdgeInsets.all(24),
                        child: Column(
                          children: [
                            Icon(Icons.bluetooth_searching, size: 48, color: Colors.grey[400]),
                            const SizedBox(height: 16),
                            const Text('No devices found'),
                            const SizedBox(height: 8),
                            const Text(
                              'Make sure your device is powered on and in pairing mode',
                              textAlign: TextAlign.center,
                              style: TextStyle(color: Colors.grey),
                            ),
                          ],
                        ),
                      ),
                    )
                  else
                    ...(_discoveredDevices.map((device) => _DeviceListItem(
                      name: device.name,
                      signal: _getSignalStrength(device.rssi),
                      isSelected: _selectedDeviceId == device.id,
                      onTap: () => _connectToDevice(device),
                    ))),
                  
                  const SizedBox(height: 16),
                  Center(
                    child: OutlinedButton.icon(
                      onPressed: _startScan,
                      icon: const Icon(Icons.refresh),
                      label: const Text('Scan Again'),
                    ),
                  ),
                ],

                if (_errorMessage != null) ...[
                  const SizedBox(height: 16),
                  Card(
                    color: Colors.red.shade50,
                    child: Padding(
                      padding: const EdgeInsets.all(12),
                      child: Row(
                        children: [
                          const Icon(Icons.error, color: Colors.red),
                          const SizedBox(width: 8),
                          Expanded(child: Text(_errorMessage!, style: const TextStyle(color: Colors.red))),
                        ],
                      ),
                    ),
                  ),
                ],
              ],
            ),
            isActive: _currentStep >= 1,
          ),

          // Step 2: Configure Network
          Step(
            title: const Text('Configure Network'),
            content: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'Choose how your device connects to the internet:',
                  style: TextStyle(fontWeight: FontWeight.bold),
                ),
                const SizedBox(height: 16),
                _NetworkOption(
                  icon: Icons.cell_tower,
                  title: 'Cellular (Recommended)',
                  subtitle: 'Uses SIM card for remote locations',
                  isSelected: _networkType == 'cellular',
                  onTap: () => setState(() => _networkType = 'cellular'),
                ),
                const SizedBox(height: 8),
                _NetworkOption(
                  icon: Icons.wifi,
                  title: 'WiFi',
                  subtitle: 'Requires nearby WiFi network',
                  isSelected: _networkType == 'wifi',
                  onTap: () => setState(() => _networkType = 'wifi'),
                ),
                const SizedBox(height: 16),
                
                if (_networkType == 'cellular')
                  TextFormField(
                    controller: _apnController,
                    decoration: const InputDecoration(
                      labelText: 'APN',
                      hintText: 'e.g., internet',
                      border: OutlineInputBorder(),
                    ),
                  )
                else ...[
                  TextFormField(
                    controller: _ssidController,
                    decoration: const InputDecoration(
                      labelText: 'WiFi Network Name',
                      border: OutlineInputBorder(),
                    ),
                  ),
                  const SizedBox(height: 12),
                  TextFormField(
                    controller: _passwordController,
                    obscureText: true,
                    decoration: const InputDecoration(
                      labelText: 'WiFi Password',
                      border: OutlineInputBorder(),
                    ),
                  ),
                ],

                if (_isProvisioning) ...[
                  const SizedBox(height: 24),
                  const Center(
                    child: Column(
                      children: [
                        CircularProgressIndicator(),
                        SizedBox(height: 16),
                        Text('Configuring device...'),
                      ],
                    ),
                  ),
                ],

                if (_errorMessage != null) ...[
                  const SizedBox(height: 16),
                  Card(
                    color: Colors.red.shade50,
                    child: Padding(
                      padding: const EdgeInsets.all(12),
                      child: Row(
                        children: [
                          const Icon(Icons.error, color: Colors.red),
                          const SizedBox(width: 8),
                          Expanded(child: Text(_errorMessage!, style: const TextStyle(color: Colors.red))),
                        ],
                      ),
                    ),
                  ),
                ],
              ],
            ),
            isActive: _currentStep >= 2,
          ),

          // Step 3: Complete
          Step(
            title: const Text('Complete Setup'),
            content: Column(
              children: [
                const Icon(Icons.check_circle, color: Colors.green, size: 64),
                const SizedBox(height: 16),
                const Text(
                  'Device Added Successfully!',
                  style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                ),
                const SizedBox(height: 8),
                const Text(
                  'Your SmartAqua feeder is now connected and ready to use.',
                  textAlign: TextAlign.center,
                ),
                const SizedBox(height: 24),
                TextFormField(
                  controller: _deviceNameController,
                  decoration: const InputDecoration(
                    labelText: 'Device Name',
                    hintText: 'e.g., Pond 1 Feeder',
                    border: OutlineInputBorder(),
                  ),
                ),
              ],
            ),
            isActive: _currentStep >= 3,
          ),
        ],
      ),
    );
  }

  bool _canContinue() {
    switch (_currentStep) {
      case 0:
        return _scannedSerialNumber != null;
      case 1:
        return _selectedDeviceId != null && !_isConnecting;
      case 2:
        return !_isProvisioning;
      default:
        return true;
    }
  }

  Widget _getButtonChild() {
    if (_currentStep == 2 && _isProvisioning) {
      return const SizedBox(
        width: 20,
        height: 20,
        child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
      );
    }
    return Text(_currentStep == 2 ? 'Configure' : 'Continue');
  }

  void _handleStepContinue() {
    if (_currentStep == 2) {
      _provisionDevice();
    } else if (_currentStep < 3) {
      setState(() => _currentStep++);
    }
  }

  void _handleStepCancel() {
    if (_currentStep > 0) {
      setState(() => _currentStep--);
    } else {
      context.pop();
    }
  }

  String _getSignalStrength(int rssi) {
    if (rssi >= -50) return 'Excellent';
    if (rssi >= -60) return 'Good';
    if (rssi >= -70) return 'Fair';
    return 'Weak';
  }
}

class _ActionCard extends StatelessWidget {
  final IconData icon;
  final String title;
  final String subtitle;
  final VoidCallback onTap;

  const _ActionCard({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(12),
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            children: [
              Icon(icon, size: 40, color: Theme.of(context).colorScheme.primary),
              const SizedBox(height: 8),
              Text(title, style: const TextStyle(fontWeight: FontWeight.bold)),
              Text(subtitle, style: TextStyle(fontSize: 12, color: Colors.grey[600])),
            ],
          ),
        ),
      ),
    );
  }
}

class _DeviceListItem extends StatelessWidget {
  final String name;
  final String signal;
  final bool isSelected;
  final VoidCallback onTap;

  const _DeviceListItem({
    required this.name,
    required this.signal,
    required this.isSelected,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      color: isSelected ? Theme.of(context).colorScheme.primaryContainer : null,
      child: ListTile(
        leading: const Icon(Icons.bluetooth),
        title: Text(name),
        subtitle: Text('Signal: $signal'),
        trailing: isSelected ? const Icon(Icons.check_circle, color: Colors.green) : null,
        onTap: onTap,
      ),
    );
  }
}

class _NetworkOption extends StatelessWidget {
  final IconData icon;
  final String title;
  final String subtitle;
  final bool isSelected;
  final VoidCallback onTap;

  const _NetworkOption({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.isSelected,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      color: isSelected ? Theme.of(context).colorScheme.primaryContainer : null,
      child: ListTile(
        leading: Icon(icon),
        title: Text(title),
        subtitle: Text(subtitle),
        trailing: isSelected 
            ? const Icon(Icons.radio_button_checked, color: Colors.green)
            : const Icon(Icons.radio_button_off),
        onTap: onTap,
      ),
    );
  }
}
