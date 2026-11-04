package org.winapk.fixture.hello;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.view.ViewTreeObserver;
import android.widget.AbsListView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/**
 * Shows the T03 screen: a greeting, a tap counter, a text field and a long list.
 *
 * Notes: every state change is logged under the tag "T03" as "key=value", the
 * lines scripts/windows/test.ps1 asserts on (PLAN.md §8 G1). The tap count is
 * kept in SharedPreferences and in a file, so a restart proves persistence.
 */
public class MainActivity extends Activity {
    private static final String TAG = "T03";
    private int taps;
    private TextView count;

    /** Builds the screen and logs the state restored from the previous run. */
    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        Log.i(TAG, "lifecycle=onCreate");
        setContentView(R.layout.main);
        final SharedPreferences prefs =getSharedPreferences("t03", MODE_PRIVATE);
        taps = prefs.getInt("taps", 0);
        Log.i(TAG, "restored.prefs=" + taps);
        Log.i(TAG, "restored.file=" + readFile());

        count = findViewById(R.id.count);
        showCount();
        Button tap = findViewById(R.id.tap);
        tap.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                taps++;
                prefs.edit().putInt("taps", taps).commit();
                writeFile(Integer.toString(taps));
                showCount();
                Log.i(TAG, "taps=" + taps);
            }
        });

        EditText input = findViewById(R.id.input);
        input.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int before, int after) {}
            @Override public void onTextChanged(CharSequence s, int start, int before, int after) {}
            @Override public void afterTextChanged(Editable s) { Log.i(TAG, "text=" + s); }
        });

        // Where the controls are, in window pixels, whenever a layout moves them: the test taps them.
        final View root = findViewById(R.id.greeting).getRootView();
        root.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
            private String last = "";
            @Override public void onGlobalLayout() {
                String now = "layout.size=" + root.getWidth() + "," + root.getHeight()
                        + " tap=" + bounds(R.id.tap) + " input=" + bounds(R.id.input) + " list=" + bounds(R.id.list);
                if (!now.equals(last)) {
                    last = now;
                    Log.i(TAG, now);
                }
            }
        });

        String[] rows = new String[100];
        for (int i = 0; i < rows.length; i++) rows[i] = "Row " + (i + 1);
        ListView list = findViewById(R.id.list);
        list.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, rows));
        list.setOnScrollListener(new AbsListView.OnScrollListener() {
            private int first = -1;
            @Override public void onScrollStateChanged(AbsListView view, int state) {}
            @Override public void onScroll(AbsListView view, int firstVisible, int visible, int total) {
                if (firstVisible != first) {
                    first = firstVisible;
                    Log.i(TAG, "list.first=" + firstVisible);
                }
            }
        });
    }

    /** Logs the resumed state. */
    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "lifecycle=onResume");
    }

    /** Logs the paused state. */
    @Override
    protected void onPause() {
        Log.i(TAG, "lifecycle=onPause");
        super.onPause();
    }

    /** Logs the back press, then finishes as the platform does. */
    @Override
    public void onBackPressed() {
        Log.i(TAG, "back=pressed");
        super.onBackPressed();
    }

    /** Logs the end of the activity. */
    @Override
    protected void onDestroy() {
        Log.i(TAG, "lifecycle=onDestroy");
        super.onDestroy();
    }

    /** Returns a view's bounds in the window as "left,top,right,bottom" pixels. */
    private String bounds(int id) {
        View v = findViewById(id);
        int[] at = new int[2];
        v.getLocationInWindow(at);
        return at[0] + "," + at[1] + "," + (at[0] + v.getWidth()) + "," + (at[1] + v.getHeight());
    }

    /** Updates the counter text. */
    private void showCount() {
        count.setText(getString(R.string.count, taps));
    }

    /** Returns the content of files/taps.txt, or "none" when there is none. */
    private String readFile() {
        File f = new File(getFilesDir(), "taps.txt");
        if (!f.exists()) return "none";
        try {
            return new String(Files.readAllBytes(f.toPath()), StandardCharsets.UTF_8);
        } catch (IOException e) {
            return "error:" + e.getMessage();
        }
    }

    /** Writes files/taps.txt. */
    private void writeFile(String value) {
        try (FileOutputStream out = new FileOutputStream(new File(getFilesDir(), "taps.txt"))) {
            out.write(value.getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) {
            Log.e(TAG, "file.error=" + e.getMessage());
        }
    }
}
