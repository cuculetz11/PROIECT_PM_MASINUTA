package com.example.landbuster;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.MotionEvent;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.UUID;

public class MainActivity extends AppCompatActivity {

    // HARDCODED HC-05 MAC ADDRESS
    private static final String MAC_ADDRESS = "00:11:22:33:44:55";
    private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

    private BluetoothAdapter bluetoothAdapter;
    private BluetoothSocket bluetoothSocket;
    private OutputStream outputStream;
    private InputStream inputStream;

    private TextView tvFata, tvSpate, tvSpeed;
    private Vibrator vibrator;
    private long lastVibrationTime = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(Bundle savedInstanceState);
        setContentView(R.layout.activity_main);

        // Cerem permisiuni pentru Android 12+ (API 31+)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this, new String[]{
                    Manifest.permission.BLUETOOTH_CONNECT,
                    Manifest.permission.BLUETOOTH_SCAN
                }, 1);
            }
        }

        tvFata = findViewById(R.id.tvFata);
        tvSpate = findViewById(R.id.tvSpate);
        tvSpeed = findViewById(R.id.tvSpeed);
        vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);

        setupButtons();
        connectBluetooth();
    }

    @SuppressLint("ClickableViewAccessibility")
    private void setupButtons() {
        Button btnUp = findViewById(R.id.btnUp);
        Button btnDown = findViewById(R.id.btnDown);
        Button btnLeft = findViewById(R.id.btnLeft);
        Button btnRight = findViewById(R.id.btnRight);
        Button btnTurbo = findViewById(R.id.btnTurbo);
        Button btnHorn = findViewById(R.id.btnHorn);
        ToggleButton btnLights = findViewById(R.id.btnLights);
        SeekBar sbSpeed = findViewById(R.id.sbSpeed);

        // D-PAD Logic (OnTouch)
        btnUp.setOnTouchListener((v, event) -> {
        if (event.getAction() == MotionEvent.ACTION_DOWN) sendCommand("w");
        else if (event.getAction() == MotionEvent.ACTION_UP) sendCommand("c");
        return false;
    });

        btnDown.setOnTouchListener((v, event) -> {
        if (event.getAction() == MotionEvent.ACTION_DOWN) sendCommand("x");
        else if (event.getAction() == MotionEvent.ACTION_UP) sendCommand("c");
        return false;
    });

        btnLeft.setOnTouchListener((v, event) -> {
        if (event.getAction() == MotionEvent.ACTION_DOWN) sendCommand("s");
        else if (event.getAction() == MotionEvent.ACTION_UP) sendCommand("a");
        return false;
    });

        btnRight.setOnTouchListener((v, event) -> {
        if (event.getAction() == MotionEvent.ACTION_DOWN) sendCommand("d");
        else if (event.getAction() == MotionEvent.ACTION_UP) sendCommand("a");
        return false;
    });

        // Viteze Slider
        sbSpeed.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                int viteza = progress + 1;
                tvSpeed.setText("Viteză: " + viteza);
                sendCommand(String.valueOf(viteza));
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        // Accesorii (Click simple)
        btnTurbo.setOnClickListener(v -> sendCommand("t"));
        btnHorn.setOnClickListener(v -> sendCommand("h"));
        btnLights.setOnClickListener(v -> sendCommand("l"));
    }

    @SuppressLint("MissingPermission")
    private void connectBluetooth() {
        bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) {
            Toast.makeText(this, "Bluetooth este oprit sau indisponibil!", Toast.LENGTH_LONG).show();
            return;
        }

        Toast.makeText(this, "Se conectează la mașină...", Toast.LENGTH_SHORT).show();

        new Thread(() -> {
        try {
            BluetoothDevice device = bluetoothAdapter.getRemoteDevice(MAC_ADDRESS);
            bluetoothSocket = device.createRfcommSocketToServiceRecord(SPP_UUID);
            bluetoothSocket.connect();

            outputStream = bluetoothSocket.getOutputStream();
            inputStream = bluetoothSocket.getInputStream();

            runOnUiThread(() -> Toast.makeText(MainActivity.this, "CONECTAT!", Toast.LENGTH_LONG).show());

            startTelemetryThread(); // Pornim ascultarea datelor dupa conectare cu succes

        } catch (IOException e) {
            e.printStackTrace();
            runOnUiThread(() -> Toast.makeText(MainActivity.this, "Conexiune eșuată! Verifică MAC-ul.", Toast.LENGTH_LONG).show());
        }
    }).start();
    }

    private void sendCommand(String cmd) {
        if (outputStream != null) {
            try {
                outputStream.write(cmd.getBytes());
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    private void startTelemetryThread() {
        new Thread(() -> {
        byte[] buffer = new byte[1024];
        int bytes;
        StringBuilder messageBuilder = new StringBuilder();

        while (bluetoothSocket != null && bluetoothSocket.isConnected()) {
            try {
                bytes = inputStream.read(buffer);
                String readMessage = new String(buffer, 0, bytes);
                messageBuilder.append(readMessage);

                // Cautam finalul de linie '\n' trimis de C++ (F:12 S:150\n)
                int endOfLineIndex = messageBuilder.indexOf("\n");
                if (endOfLineIndex > 0) {
                    String data = messageBuilder.substring(0, endOfLineIndex).trim();
                    messageBuilder.delete(0, endOfLineIndex + 1);

                    if (data.startsWith("F:")) {
                        parseAndDisplayTelemetry(data);
                    }
                }
            } catch (IOException e) {
                runOnUiThread(() -> Toast.makeText(MainActivity.this, "Conexiune pierdută!", Toast.LENGTH_SHORT).show());
                break;
            }
        }
    }).start();
    }

    private void parseAndDisplayTelemetry(String data) {
        try {
            // Format asteptat: "F:12 S:150"
            String[] parts = data.split(" ");
            if (parts.length >= 2) {
                String fa = parts[0].replace("F:", "");
                String sp = parts[1].replace("S:", "");

                final int distFata = Integer.parseInt(fa);
                final int distSpate = Integer.parseInt(sp);

                runOnUiThread(() -> {
                    tvFata.setText("Distanță Față: " + (distFata == 999 ? "Liber" : distFata + " cm"));
                    tvSpate.setText("Distanță Spate: " + (distSpate == 999 ? "Liber" : distSpate + " cm"));
                });

                // Haptic Feedback la sub 15cm (dar limitat la maxim 1 vibratie pe secunda ca sa nu blocam motorasul haptic)
                if ((distFata < 15 || distSpate < 15) && (System.currentTimeMillis() - lastVibrationTime > 1000)) {
                    lastVibrationTime = System.currentTimeMillis();
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        vibrator.vibrate(VibrationEffect.createOneShot(200, VibrationEffect.DEFAULT_AMPLITUDE));
                    } else {
                        vibrator.vibrate(200);
                    }
                }
            }
        } catch (Exception e) {
            e.printStackTrace(); // Ignoram parsari gresite ocazionale pe Serial
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        try {
            if (bluetoothSocket != null) bluetoothSocket.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
