import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

// ── Биты кнопок (совпадают с BTN_* в AleksOS config.h) ──────────────
const int BTN_A     = 0x01;
const int BTN_B     = 0x02;
const int BTN_SEL   = 0x04;
const int BTN_STA   = 0x08;
const int BTN_UP    = 0x10;
const int BTN_DOWN  = 0x20;
const int BTN_LEFT  = 0x40;
const int BTN_RIGHT = 0x80;

// Nordic UART Service RX — характеристика для записи с телефона
const String _NUS_RX = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';

const String _appVersion = 'v1.0';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  SystemChrome.setPreferredOrientations([
    DeviceOrientation.landscapeLeft,
    DeviceOrientation.landscapeRight,
  ]);
  SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
  runApp(const AleksOSControllerApp());
}

class AleksOSControllerApp extends StatelessWidget {
  const AleksOSControllerApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'AleksOS Controller',
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark().copyWith(
        scaffoldBackgroundColor: const Color(0xFF0D0D0D),
        colorScheme: const ColorScheme.dark(
          primary: Color(0xFFFFB300),
          surface: Color(0xFF1A1A1A),
        ),
      ),
      home: const ControllerScreen(),
    );
  }
}

class ControllerScreen extends StatefulWidget {
  const ControllerScreen({super.key});

  @override
  State<ControllerScreen> createState() => _ControllerScreenState();
}

class _ControllerScreenState extends State<ControllerScreen> {
  BluetoothDevice?       _device;
  BluetoothCharacteristic? _char;
  StreamSubscription?    _connSub;
  bool   _isConnecting = false;
  bool   _isConnected  = false;
  String _statusText   = 'Не подключено';
  int    _btnState     = 0;

  @override
  void dispose() {
    _connSub?.cancel();
    _device?.disconnect();
    super.dispose();
  }

  Future<bool> _requestPermissions() async {
    final perms = <Permission>[
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
    ];
    // Android < 12 требует Location для BLE-сканирования
    if (await Permission.location.status.isDenied) {
      perms.add(Permission.location);
    }
    final statuses = await perms.request();
    return statuses[Permission.bluetoothScan]!.isGranted &&
           statuses[Permission.bluetoothConnect]!.isGranted;
  }

  Future<void> _connect() async {
    if (_isConnecting || _isConnected) return;

    final ok = await _requestPermissions();
    if (!ok) {
      setState(() => _statusText = 'Нет разрешений Bluetooth');
      return;
    }

    setState(() {
      _isConnecting = true;
      _statusText   = 'Сканирование (5 сек)...';
    });

    // Собираем уже сопряжённые BLE-устройства (bonded) — они могут не появляться в scanResults
    List<BluetoothDevice> bonded = [];
    try {
      bonded = await FlutterBluePlus.bondedDevices;
    } catch (_) {}

    // Сканируем новые устройства
    List<ScanResult> scanned = [];
    try {
      final sub = FlutterBluePlus.scanResults.listen((r) => scanned = List.from(r));
      await FlutterBluePlus.startScan(timeout: const Duration(seconds: 5));
      await FlutterBluePlus.stopScan();
      await sub.cancel();
    } catch (e) {
      setState(() { _isConnecting = false; _statusText = 'Ошибка сканирования'; });
      return;
    }

    if (!mounted) return;

    // Объединяем: bonded + scanned (без дублей)
    final allDevices = <BluetoothDevice>[
      ...bonded,
      ...scanned.map((r) => r.device).where((d) => !bonded.any((b) => b.remoteId == d.remoteId)),
    ];

    // Только устройства с именем; AleksOS первым
    final visible = allDevices
        .where((d) => d.platformName.isNotEmpty)
        .toList()
      ..sort((a, b) {
        if (a.platformName == 'AleksOS') return -1;
        if (b.platformName == 'AleksOS') return 1;
        return a.platformName.compareTo(b.platformName);
      });

    final selected = await showDialog<BluetoothDevice>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF1A1A1A),
        title: const Text('Выбери консоль',
            style: TextStyle(color: Color(0xFFFFB300))),
        content: SizedBox(
          width: 300,
          child: visible.isEmpty
              ? const Text(
                  'Устройства не найдены.\nУбедись что AleksOS включён\nи Bluetooth активен.\n\nЕсли консоль была сопряжена через\nнастройки телефона — она появится\nв списке автоматически.',
                  style: TextStyle(color: Colors.white70))
              : ListView.builder(
                  shrinkWrap: true,
                  itemCount: visible.length,
                  itemBuilder: (_, i) {
                    final isBonded = bonded.any((b) => b.remoteId == visible[i].remoteId);
                    return ListTile(
                      leading: Icon(
                        isBonded ? Icons.link : Icons.gamepad,
                        color: const Color(0xFFFFB300),
                      ),
                      title: Text(visible[i].platformName,
                          style: const TextStyle(color: Colors.white)),
                      subtitle: Text(
                        '${visible[i].remoteId.str}${isBonded ? ' · сопряжено' : ''}',
                        style: const TextStyle(color: Colors.white54, fontSize: 12),
                      ),
                      onTap: () => Navigator.pop(ctx, visible[i]),
                    );
                  },
                ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text('Отмена', style: TextStyle(color: Colors.white54)),
          ),
        ],
      ),
    );

    if (selected == null) {
      setState(() { _isConnecting = false; _statusText = 'Не подключено'; });
      return;
    }

    setState(() => _statusText = 'Подключаюсь к ${selected.platformName}...');

    try {
      await selected.connect(timeout: const Duration(seconds: 10));

      _connSub = selected.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) _onDisconnect();
      });

      final services = await selected.discoverServices();
      BluetoothCharacteristic? ch;
      outer:
      for (final svc in services) {
        for (final c in svc.characteristics) {
          if (c.uuid.toString().toLowerCase() == _NUS_RX) {
            ch = c;
            break outer;
          }
        }
      }

      if (ch == null) {
        await selected.disconnect();
        setState(() {
          _isConnecting = false;
          _statusText   = 'Геймпад-характеристика не найдена';
        });
        return;
      }

      setState(() {
        _device      = selected;
        _char        = ch;
        _isConnected = true;
        _isConnecting = false;
        _statusText  = selected.platformName;
      });
    } catch (e) {
      setState(() { _isConnecting = false; _statusText = 'Ошибка подключения'; });
    }
  }

  void _onDisconnect() {
    if (!mounted) return;
    _connSub?.cancel();
    _connSub = null;
    setState(() {
      _device      = null;
      _char        = null;
      _isConnected = false;
      _btnState    = 0;
      _statusText  = 'Отключено';
    });
  }

  void _disconnect() {
    _device?.disconnect();
    _onDisconnect();
  }

  void _sendState() {
    if (!_isConnected || _char == null) return;
    _char!.write([_btnState], withoutResponse: true).catchError((_) {});
  }

  void _pressBtn(int mask) {
    _btnState |= mask;
    _sendState();
    setState(() {});
  }

  void _releaseBtn(int mask) {
    _btnState &= ~mask;
    _sendState();
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Stack(
        children: [
          // Фон
          Container(
            decoration: const BoxDecoration(
              gradient: LinearGradient(
                begin: Alignment.topCenter,
                end: Alignment.bottomCenter,
                colors: [Color(0xFF111111), Color(0xFF0A0A0A)],
              ),
            ),
          ),

          // Верхняя полоса
          Positioned(
            top: 0, left: 0, right: 0,
            child: _buildTopBar(),
          ),

          // SELECT и START по центру
          Center(
            child: Padding(
              padding: const EdgeInsets.only(top: 36),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  _buildSmallBtn('SELECT', BTN_SEL),
                  const SizedBox(width: 28),
                  _buildSmallBtn('START', BTN_STA),
                ],
              ),
            ),
          ),

          // D-pad слева
          Positioned(
            left: 24, top: 0, bottom: 0,
            child: Center(child: _buildDpad()),
          ),

          // A и B справа
          Positioned(
            right: 28, top: 0, bottom: 0,
            child: Center(child: _buildAB()),
          ),
        ],
      ),
    );
  }

  Widget _buildTopBar() {
    return Container(
      height: 36,
      padding: const EdgeInsets.symmetric(horizontal: 12),
      color: const Color(0xFF1A1A1A),
      child: Row(
        children: [
          Icon(
            _isConnected ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
            color: _isConnected ? const Color(0xFFFFB300) : Colors.white38,
            size: 18,
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              _statusText,
              style: TextStyle(
                color: _isConnected ? Colors.white : Colors.white54,
                fontSize: 13,
              ),
              overflow: TextOverflow.ellipsis,
            ),
          ),
          const SizedBox(width: 6),
          const Text(_appVersion,
              style: TextStyle(color: Colors.white24, fontSize: 10)),
          const SizedBox(width: 6),
          if (_isConnected)
            GestureDetector(
              onTap: _disconnect,
              child: const Padding(
                padding: EdgeInsets.symmetric(horizontal: 8),
                child: Icon(Icons.close, color: Colors.white38, size: 18),
              ),
            )
          else
            GestureDetector(
              onTap: _isConnecting ? null : _connect,
              child: Container(
                padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 4),
                decoration: BoxDecoration(
                  color: _isConnecting
                      ? Colors.white12
                      : const Color(0xFFFFB300).withValues(alpha: 0.15),
                  borderRadius: BorderRadius.circular(6),
                  border: Border.all(
                    color: _isConnecting
                        ? Colors.white24
                        : const Color(0xFFFFB300),
                    width: 1,
                  ),
                ),
                child: Text(
                  _isConnecting ? 'Подключение...' : 'Подключить',
                  style: TextStyle(
                    color: _isConnecting
                        ? Colors.white38
                        : const Color(0xFFFFB300),
                    fontSize: 13,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ),
            ),
        ],
      ),
    );
  }

  Widget _buildDpad() {
    const size = 66.0;
    const gap  = 4.0;
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        _dpadBtn(BTN_UP, Icons.keyboard_arrow_up, size),
        const SizedBox(height: gap),
        Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            _dpadBtn(BTN_LEFT, Icons.keyboard_arrow_left, size),
            const SizedBox(width: gap),
            Container(
              width: size, height: size,
              decoration: BoxDecoration(
                color: const Color(0xFF181818),
                borderRadius: BorderRadius.circular(8),
              ),
            ),
            const SizedBox(width: gap),
            _dpadBtn(BTN_RIGHT, Icons.keyboard_arrow_right, size),
          ],
        ),
        const SizedBox(height: gap),
        _dpadBtn(BTN_DOWN, Icons.keyboard_arrow_down, size),
      ],
    );
  }

  Widget _dpadBtn(int mask, IconData icon, double size) {
    final pressed = (_btnState & mask) != 0;
    return GestureDetector(
      onTapDown:  (_) => _pressBtn(mask),
      onTapUp:    (_) => _releaseBtn(mask),
      onTapCancel: () => _releaseBtn(mask),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 50),
        width: size, height: size,
        decoration: BoxDecoration(
          color: pressed ? const Color(0xFF2A2A2A) : const Color(0xFF1C1C1C),
          borderRadius: BorderRadius.circular(8),
          border: Border.all(
            color: pressed ? const Color(0xFFFFB300) : const Color(0xFF2E2E2E),
            width: pressed ? 2 : 1,
          ),
          boxShadow: pressed
              ? []
              : [BoxShadow(
                  color: Colors.black.withValues(alpha: 0.6),
                  blurRadius: 4, offset: const Offset(0, 3))],
        ),
        child: Icon(icon,
            color: pressed ? const Color(0xFFFFB300) : Colors.white38,
            size: 34),
      ),
    );
  }

  Widget _buildAB() {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        _roundBtn('B', BTN_B, const Color(0xFFE53935)),
        const SizedBox(height: 20),
        _roundBtn('A', BTN_A, const Color(0xFF43A047)),
      ],
    );
  }

  Widget _roundBtn(String label, int mask, Color color) {
    final pressed = (_btnState & mask) != 0;
    return GestureDetector(
      onTapDown:  (_) => _pressBtn(mask),
      onTapUp:    (_) => _releaseBtn(mask),
      onTapCancel: () => _releaseBtn(mask),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 50),
        width: 70, height: 70,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          color: pressed ? color : color.withValues(alpha: 0.15),
          border: Border.all(color: color, width: 2),
          boxShadow: pressed
              ? [BoxShadow(color: color.withValues(alpha: 0.5), blurRadius: 10)]
              : [BoxShadow(
                  color: Colors.black.withValues(alpha: 0.6),
                  blurRadius: 4, offset: const Offset(0, 3))],
        ),
        child: Center(
          child: Text(label,
            style: TextStyle(
              color: pressed ? Colors.white : color,
              fontSize: 24,
              fontWeight: FontWeight.bold,
            )),
        ),
      ),
    );
  }

  Widget _buildSmallBtn(String label, int mask) {
    final pressed = (_btnState & mask) != 0;
    return GestureDetector(
      onTapDown:  (_) => _pressBtn(mask),
      onTapUp:    (_) => _releaseBtn(mask),
      onTapCancel: () => _releaseBtn(mask),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 50),
        padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 8),
        decoration: BoxDecoration(
          color: pressed
              ? const Color(0xFFFFB300).withValues(alpha: 0.25)
              : const Color(0xFF1C1C1C),
          borderRadius: BorderRadius.circular(20),
          border: Border.all(
            color: pressed ? const Color(0xFFFFB300) : const Color(0xFF2E2E2E),
            width: pressed ? 2 : 1,
          ),
        ),
        child: Text(label,
          style: TextStyle(
            color: pressed ? const Color(0xFFFFB300) : Colors.white38,
            fontSize: 12,
            fontWeight: FontWeight.w700,
            letterSpacing: 1.2,
          )),
      ),
    );
  }
}
