import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fl_chart/fl_chart.dart';

import '../../../../core/theme/app_theme.dart';
import '../../../../core/models/device.dart';
import '../../../../core/models/sensor_data.dart';
import '../../../../core/providers/device_provider.dart';
import '../../../../core/providers/monitoring_provider.dart';

class MonitoringScreen extends ConsumerStatefulWidget {
  const MonitoringScreen({super.key});

  @override
  ConsumerState<MonitoringScreen> createState() => _MonitoringScreenState();
}

class _MonitoringScreenState extends ConsumerState<MonitoringScreen> {
  String? _selectedDeviceId;

  @override
  void initState() {
    super.initState();
    _loadData();
  }

  Future<void> _loadData() async {
    await ref.read(deviceListProvider.notifier).loadDevices();
    final devices = ref.read(devicesProvider);
    if (devices.isNotEmpty && _selectedDeviceId == null) {
      _selectedDeviceId = devices.first.id;
      await _loadDeviceData();
    }
  }

  Future<void> _loadDeviceData() async {
    if (_selectedDeviceId == null) return;
    await Future.wait([
      ref.read(sensorDataProvider.notifier).loadSensorData(_selectedDeviceId!),
      ref.read(alertsProvider.notifier).loadAlerts(_selectedDeviceId!),
    ]);
  }

  @override
  Widget build(BuildContext context) {
    final deviceState = ref.watch(deviceListProvider);
    final sensorState = ref.watch(sensorDataProvider);
    final alertsState = ref.watch(alertsProvider);
    final sensorData = sensorState.currentData;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Monitoring'),
      ),
      body: RefreshIndicator(
        onRefresh: _loadDeviceData,
        child: SingleChildScrollView(
          physics: const AlwaysScrollableScrollPhysics(),
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // Device selector
              Card(
                child: ListTile(
                  leading: const Icon(Icons.router),
                  title: Text(_getSelectedDeviceName(deviceState.devices)),
                  subtitle: Text(sensorState.lastUpdated != null 
                      ? 'Last updated: ${_formatTime(sensorState.lastUpdated!)}'
                      : 'Select a device'),
                  trailing: const Icon(Icons.arrow_drop_down),
                  onTap: () => _showDeviceSelector(context, deviceState.devices),
                ),
              ),
              const SizedBox(height: 16),

              Text(
                'Sensor Readings',
                style: Theme.of(context).textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.bold,
                ),
              ),
              const SizedBox(height: 12),

              if (sensorState.isLoading)
                const Center(child: Padding(
                  padding: EdgeInsets.all(32),
                  child: CircularProgressIndicator(),
                ))
              else if (sensorData == null)
                _buildNoDataCard(context)
              else ...[
                Row(
                  children: [
                    Expanded(
                      child: _SensorCard(
                        icon: Icons.thermostat,
                        label: 'Water Temp',
                        value: '${sensorData.waterTemperature.toStringAsFixed(1)}°C',
                        status: _getTempStatus(sensorData.waterTemperature),
                        statusColor: _getTempColor(sensorData.waterTemperature),
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: _SensorCard(
                        icon: Icons.inventory_2,
                        label: 'Feed Level',
                        value: '${sensorData.feedLevel.toInt()}%',
                        status: _getFeedStatus(sensorData.feedLevel),
                        statusColor: _getFeedColor(sensorData.feedLevel),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 12),
                Row(
                  children: [
                    Expanded(
                      child: _SensorCard(
                        icon: Icons.battery_charging_full,
                        label: 'Battery',
                        value: '${sensorData.batteryLevel.toInt()}%',
                        status: sensorData.isSolarCharging ? 'Charging' : 'Discharging',
                        statusColor: _getBatteryColor(sensorData.batteryLevel),
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: _SensorCard(
                        icon: Icons.wb_sunny,
                        label: 'Solar',
                        value: '${sensorData.solarVoltage.toStringAsFixed(1)}V',
                        status: sensorData.isSolarCharging ? 'Active' : 'Inactive',
                        statusColor: sensorData.isSolarCharging 
                            ? AppTheme.solarActive 
                            : Colors.grey,
                      ),
                    ),
                  ],
                ),
                if (sensorData.dissolvedOxygen != null || sensorData.ph != null) ...[
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      if (sensorData.dissolvedOxygen != null)
                        Expanded(
                          child: _SensorCard(
                            icon: Icons.air,
                            label: 'Dissolved O₂',
                            value: '${sensorData.dissolvedOxygen!.toStringAsFixed(1)} mg/L',
                            status: _getOxygenStatus(sensorData.dissolvedOxygen!),
                            statusColor: _getOxygenColor(sensorData.dissolvedOxygen!),
                          ),
                        ),
                      if (sensorData.dissolvedOxygen != null && sensorData.ph != null)
                        const SizedBox(width: 12),
                      if (sensorData.ph != null)
                        Expanded(
                          child: _SensorCard(
                            icon: Icons.science,
                            label: 'pH Level',
                            value: sensorData.ph!.toStringAsFixed(1),
                            status: _getPhStatus(sensorData.ph!),
                            statusColor: _getPhColor(sensorData.ph!),
                          ),
                        ),
                    ],
                  ),
                ],
                const SizedBox(height: 12),
                // Connection info
                Card(
                  child: ListTile(
                    leading: Icon(
                      sensorData.connectionType == 'gsm' ? Icons.signal_cellular_alt : Icons.wifi,
                      color: _getSignalColor(sensorData.signalStrength),
                    ),
                    title: Text(sensorData.connectionType.toUpperCase()),
                    subtitle: Text('Signal: ${sensorData.signalStrength}%'),
                    trailing: Icon(
                      Icons.circle,
                      size: 12,
                      color: sensorData.signalStrength > 50 ? Colors.green : Colors.orange,
                    ),
                  ),
                ),

                // Q10 Status Card
                const SizedBox(height: 16),
                _Q10StatusCard(
                  temperature: sensorData.waterTemperature,
                  dissolvedOxygen: sensorData.dissolvedOxygen,
                ),

                // FCR Tracking Card
                const SizedBox(height: 16),
                _FCRTrackingCard(deviceId: _selectedDeviceId!),
              ],

              const SizedBox(height: 24),
              Text(
                'Alerts',
                style: Theme.of(context).textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.bold,
                ),
              ),
              const SizedBox(height: 12),

              if (alertsState.isLoading)
                const Center(child: CircularProgressIndicator())
              else if (alertsState.alerts.isEmpty)
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(24),
                    child: Center(
                      child: Column(
                        children: [
                          Icon(Icons.check_circle, size: 48, color: Colors.green[300]),
                          const SizedBox(height: 8),
                          Text('No alerts', style: TextStyle(color: Colors.grey[600])),
                        ],
                      ),
                    ),
                  ),
                )
              else
                ...alertsState.alerts.take(5).map((alert) => _AlertCard(
                  title: alert.title,
                  message: alert.message,
                  time: _formatTime(alert.createdAt),
                  severity: alert.severity,
                )),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildNoDataCard(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Center(
          child: Column(
            children: [
              Icon(Icons.sensors_off, size: 48, color: Colors.grey[400]),
              const SizedBox(height: 8),
              Text('No sensor data available', style: TextStyle(color: Colors.grey[600])),
            ],
          ),
        ),
      ),
    );
  }

  String _getSelectedDeviceName(List<Device> devices) {
    if (_selectedDeviceId == null) return 'No device selected';
    try {
      return devices.firstWhere((d) => d.id == _selectedDeviceId).name;
    } catch (_) {
      return 'Unknown device';
    }
  }

  void _showDeviceSelector(BuildContext context, List<Device> devices) {
    showModalBottomSheet(
      context: context,
      builder: (context) => ListView(
        shrinkWrap: true,
        children: [
          const Padding(
            padding: EdgeInsets.all(16),
            child: Text('Select Device', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 18)),
          ),
          ...devices.map((device) => ListTile(
            leading: Icon(Icons.router, color: device.isOnline ? Colors.green : Colors.grey),
            title: Text(device.name),
            subtitle: Text(device.serialNumber),
            trailing: _selectedDeviceId == device.id 
                ? const Icon(Icons.check, color: Colors.green)
                : null,
            onTap: () {
              setState(() => _selectedDeviceId = device.id);
              _loadDeviceData();
              Navigator.pop(context);
            },
          )),
        ],
      ),
    );
  }

  String _formatTime(DateTime time) {
    final diff = DateTime.now().difference(time);
    if (diff.inMinutes < 1) return 'Just now';
    if (diff.inMinutes < 60) return '${diff.inMinutes}m ago';
    if (diff.inHours < 24) return '${diff.inHours}h ago';
    return '${diff.inDays}d ago';
  }

  // Status helpers
  String _getTempStatus(double temp) {
    if (temp < 20) return 'Low';
    if (temp > 32) return 'High';
    return 'Normal';
  }

  Color _getTempColor(double temp) {
    if (temp < 20 || temp > 32) return Colors.orange;
    return AppTheme.waterTempNormal;
  }

  String _getFeedStatus(double level) {
    if (level < 20) return 'Critical';
    if (level < 50) return 'Low';
    return 'Good';
  }

  Color _getFeedColor(double level) {
    if (level > 50) return AppTheme.feedLevelHigh;
    if (level > 20) return AppTheme.feedLevelMedium;
    return AppTheme.feedLevelLow;
  }

  Color _getBatteryColor(double level) {
    if (level > 50) return AppTheme.batteryFull;
    if (level > 20) return AppTheme.batteryMedium;
    return AppTheme.batteryLow;
  }

  String _getOxygenStatus(double do2) {
    if (do2 < 3) return 'Critical';
    if (do2 < 5) return 'Low';
    return 'Normal';
  }

  Color _getOxygenColor(double do2) {
    if (do2 < 3) return Colors.red;
    if (do2 < 5) return Colors.orange;
    return Colors.green;
  }

  String _getPhStatus(double ph) {
    if (ph < 6.5 || ph > 8.5) return 'Warning';
    return 'Normal';
  }

  Color _getPhColor(double ph) {
    if (ph < 6.5 || ph > 8.5) return Colors.orange;
    return Colors.green;
  }

  Color _getSignalColor(int strength) {
    if (strength > 70) return Colors.green;
    if (strength > 40) return Colors.orange;
    return Colors.red;
  }
}

class _SensorCard extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;
  final String status;
  final Color statusColor;

  const _SensorCard({
    required this.icon,
    required this.label,
    required this.value,
    required this.status,
    required this.statusColor,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(icon, color: statusColor, size: 24),
                const SizedBox(width: 8),
                Text(
                  label,
                  style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    color: Colors.grey,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Text(
              value,
              style: Theme.of(context).textTheme.headlineSmall?.copyWith(
                fontWeight: FontWeight.bold,
              ),
            ),
            Text(
              status,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: statusColor,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _AlertCard extends StatelessWidget {
  final String title;
  final String message;
  final String time;
  final AlertSeverity severity;

  const _AlertCard({
    required this.title,
    required this.message,
    required this.time,
    required this.severity,
  });

  Color get _color {
    switch (severity) {
      case AlertSeverity.info:
        return Colors.blue;
      case AlertSeverity.warning:
        return Colors.orange;
      case AlertSeverity.critical:
        return Colors.red;
    }
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ListTile(
        leading: CircleAvatar(
          backgroundColor: _color.withValues(alpha: 0.2),
          child: Icon(Icons.notifications, color: _color),
        ),
        title: Text(title),
        subtitle: Text(message),
        trailing: Text(time, style: Theme.of(context).textTheme.bodySmall),
      ),
    );
  }
}

/// Q10 Metabolic Status Visualization
class _Q10StatusCard extends StatelessWidget {
  final double temperature;
  final double? dissolvedOxygen;

  const _Q10StatusCard({
    required this.temperature,
    this.dissolvedOxygen,
  });

  // Q10 calculation: Q10^((T - Tref) / 10)
  double _calculateQ10Factor(double temp, {double q10 = 2.2, double tRef = 25.0}) {
    return q10 * ((temp - tRef) / 10);
  }

  // OBM Safety Factor: max(0, (DO - DO_lethal) / (DO_optimal - DO_lethal))
  double _calculateOBMFactor(double? do2, {double doOptimal = 7.0, double doLethal = 2.0}) {
    if (do2 == null) return 1.0;
    if (do2 <= doLethal) return 0.0;
    if (do2 >= doOptimal) return 1.0;
    return (do2 - doLethal) / (doOptimal - doLethal);
  }

  String _getMetabolicStatus(double temp) {
    if (temp < 18) return 'Low Metabolism';
    if (temp > 32) return 'Thermal Stress';
    if (temp >= 25 && temp <= 30) return 'Optimal';
    return 'Moderate';
  }

  Color _getMetabolicColor(double temp) {
    if (temp < 18 || temp > 32) return Colors.red;
    if (temp >= 25 && temp <= 30) return Colors.green;
    return Colors.orange;
  }

  @override
  Widget build(BuildContext context) {
    final q10Factor = _calculateQ10Factor(temperature);
    final obmFactor = _calculateOBMFactor(dissolvedOxygen);
    final combinedFactor = q10Factor * obmFactor;
    final metabolicStatus = _getMetabolicStatus(temperature);
    final statusColor = _getMetabolicColor(temperature);

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.science, color: Theme.of(context).colorScheme.primary),
                const SizedBox(width: 8),
                Text(
                  'Q10 Metabolic Status',
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(fontWeight: FontWeight.bold),
                ),
              ],
            ),
            const SizedBox(height: 16),
            
            // Metabolic gauge
            SizedBox(
              height: 120,
              child: Stack(
                alignment: Alignment.center,
                children: [
                  SizedBox(
                    width: 100,
                    height: 100,
                    child: CircularProgressIndicator(
                      value: (combinedFactor.clamp(0, 2) / 2),
                      strokeWidth: 12,
                      backgroundColor: Colors.grey.shade200,
                      valueColor: AlwaysStoppedAnimation(statusColor),
                    ),
                  ),
                  Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Text(
                        combinedFactor.toStringAsFixed(2),
                        style: Theme.of(context).textTheme.headlineSmall?.copyWith(fontWeight: FontWeight.bold),
                      ),
                      Text('Factor', style: TextStyle(color: Colors.grey[600], fontSize: 12)),
                    ],
                  ),
                ],
              ),
            ),
            
            const SizedBox(height: 16),
            
            // Status indicators
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
              decoration: BoxDecoration(
                color: statusColor.withValues(alpha: 0.1),
                borderRadius: BorderRadius.circular(8),
              ),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Icon(Icons.circle, size: 8, color: statusColor),
                  const SizedBox(width: 8),
                  Text(metabolicStatus, style: TextStyle(color: statusColor, fontWeight: FontWeight.bold)),
                ],
              ),
            ),
            
            const SizedBox(height: 16),
            
            // Factor breakdown
            Row(
              children: [
                Expanded(
                  child: _FactorItem(
                    label: 'Q10 Factor',
                    value: q10Factor.toStringAsFixed(2),
                    icon: Icons.thermostat,
                    color: _getMetabolicColor(temperature),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: _FactorItem(
                    label: 'OBM Safety',
                    value: '${(obmFactor * 100).toInt()}%',
                    icon: Icons.air,
                    color: obmFactor > 0.7 ? Colors.green : obmFactor > 0.3 ? Colors.orange : Colors.red,
                  ),
                ),
              ],
            ),
            
            if (dissolvedOxygen != null && dissolvedOxygen! < 3.0) ...[
              const SizedBox(height: 12),
              Container(
                padding: const EdgeInsets.all(8),
                decoration: BoxDecoration(
                  color: Colors.red.withValues(alpha: 0.1),
                  borderRadius: BorderRadius.circular(8),
                  border: Border.all(color: Colors.red.withValues(alpha: 0.3)),
                ),
                child: const Row(
                  children: [
                    Icon(Icons.warning, color: Colors.red, size: 20),
                    SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        'Critical DO level! Feeding suspended.',
                        style: TextStyle(color: Colors.red, fontSize: 12),
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _FactorItem extends StatelessWidget {
  final String label;
  final String value;
  final IconData icon;
  final Color color;

  const _FactorItem({
    required this.label,
    required this.value,
    required this.icon,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Colors.grey.shade100,
        borderRadius: BorderRadius.circular(8),
      ),
      child: Column(
        children: [
          Icon(icon, color: color, size: 20),
          const SizedBox(height: 4),
          Text(value, style: TextStyle(fontWeight: FontWeight.bold, color: color)),
          Text(label, style: TextStyle(fontSize: 11, color: Colors.grey[600])),
        ],
      ),
    );
  }
}

/// FCR Tracking Card with Chart
class _FCRTrackingCard extends ConsumerWidget {
  final String deviceId;

  const _FCRTrackingCard({required this.deviceId});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    // Mock FCR data - in production, this would come from a provider
    final fcrData = [
      FlSpot(0, 1.8),
      FlSpot(1, 1.7),
      FlSpot(2, 1.6),
      FlSpot(3, 1.5),
      FlSpot(4, 1.4),
      FlSpot(5, 1.3),
      FlSpot(6, 1.25),
    ];
    
    final currentFCR = fcrData.last.y;
    final targetFCR = 1.2;
    final improvement = ((1.8 - currentFCR) / 1.8 * 100).toStringAsFixed(1);

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.trending_down, color: Theme.of(context).colorScheme.primary),
                const SizedBox(width: 8),
                Text(
                  'FCR Tracking',
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(fontWeight: FontWeight.bold),
                ),
                const Spacer(),
                Container(
                  padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                  decoration: BoxDecoration(
                    color: Colors.green.withValues(alpha: 0.1),
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: Text(
                    '↓ $improvement%',
                    style: const TextStyle(color: Colors.green, fontWeight: FontWeight.bold, fontSize: 12),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 16),
            
            // Current FCR display
            Row(
              children: [
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('Current FCR', style: TextStyle(color: Colors.grey[600], fontSize: 12)),
                      Text(
                        currentFCR.toStringAsFixed(2),
                        style: Theme.of(context).textTheme.headlineMedium?.copyWith(
                          fontWeight: FontWeight.bold,
                          color: currentFCR <= 1.3 ? Colors.green : currentFCR <= 1.5 ? Colors.orange : Colors.red,
                        ),
                      ),
                    ],
                  ),
                ),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('Target FCR', style: TextStyle(color: Colors.grey[600], fontSize: 12)),
                      Text(
                        targetFCR.toStringAsFixed(2),
                        style: Theme.of(context).textTheme.headlineMedium?.copyWith(
                          fontWeight: FontWeight.bold,
                          color: Colors.blue,
                        ),
                      ),
                    ],
                  ),
                ),
              ],
            ),
            
            const SizedBox(height: 16),
            
            // FCR trend chart
            SizedBox(
              height: 150,
              child: LineChart(
                LineChartData(
                  gridData: FlGridData(
                    show: true,
                    drawVerticalLine: false,
                    horizontalInterval: 0.2,
                    getDrawingHorizontalLine: (value) => FlLine(
                      color: Colors.grey.shade200,
                      strokeWidth: 1,
                    ),
                  ),
                  titlesData: FlTitlesData(
                    leftTitles: AxisTitles(
                      sideTitles: SideTitles(
                        showTitles: true,
                        reservedSize: 30,
                        getTitlesWidget: (value, meta) => Text(
                          value.toStringAsFixed(1),
                          style: TextStyle(fontSize: 10, color: Colors.grey[600]),
                        ),
                      ),
                    ),
                    bottomTitles: AxisTitles(
                      sideTitles: SideTitles(
                        showTitles: true,
                        getTitlesWidget: (value, meta) => Text(
                          'W${value.toInt() + 1}',
                          style: TextStyle(fontSize: 10, color: Colors.grey[600]),
                        ),
                      ),
                    ),
                    topTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                    rightTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                  ),
                  borderData: FlBorderData(show: false),
                  minX: 0,
                  maxX: 6,
                  minY: 1.0,
                  maxY: 2.0,
                  lineBarsData: [
                    // Target line
                    LineChartBarData(
                      spots: [const FlSpot(0, 1.2), const FlSpot(6, 1.2)],
                      isCurved: false,
                      color: Colors.blue.withValues(alpha: 0.5),
                      barWidth: 2,
                      dotData: const FlDotData(show: false),
                      dashArray: [5, 5],
                    ),
                    // Actual FCR
                    LineChartBarData(
                      spots: fcrData,
                      isCurved: true,
                      color: Colors.green,
                      barWidth: 3,
                      dotData: FlDotData(
                        show: true,
                        getDotPainter: (spot, percent, barData, index) => FlDotCirclePainter(
                          radius: 4,
                          color: Colors.green,
                          strokeWidth: 2,
                          strokeColor: Colors.white,
                        ),
                      ),
                      belowBarData: BarAreaData(
                        show: true,
                        color: Colors.green.withValues(alpha: 0.1),
                      ),
                    ),
                  ],
                ),
              ),
            ),
            
            const SizedBox(height: 12),
            
            // Legend
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                _LegendItem(color: Colors.green, label: 'Actual FCR'),
                const SizedBox(width: 16),
                _LegendItem(color: Colors.blue, label: 'Target (1.2)', dashed: true),
              ],
            ),
            
            const SizedBox(height: 12),
            
            // Recommendation
            Container(
              padding: const EdgeInsets.all(8),
              decoration: BoxDecoration(
                color: Colors.blue.withValues(alpha: 0.1),
                borderRadius: BorderRadius.circular(8),
              ),
              child: Row(
                children: [
                  const Icon(Icons.lightbulb_outline, size: 16, color: Colors.blue),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      currentFCR <= targetFCR
                          ? 'Excellent! FCR is at optimal level.'
                          : 'Tip: Optimize feeding times during peak metabolic hours.',
                      style: const TextStyle(fontSize: 12, color: Colors.blue),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _LegendItem extends StatelessWidget {
  final Color color;
  final String label;
  final bool dashed;

  const _LegendItem({required this.color, required this.label, this.dashed = false});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Container(
          width: 16,
          height: 3,
          decoration: BoxDecoration(
            color: dashed ? Colors.transparent : color,
            border: dashed ? Border(bottom: BorderSide(color: color, width: 2, style: BorderStyle.solid)) : null,
          ),
        ),
        const SizedBox(width: 4),
        Text(label, style: TextStyle(fontSize: 11, color: Colors.grey[600])),
      ],
    );
  }
}
