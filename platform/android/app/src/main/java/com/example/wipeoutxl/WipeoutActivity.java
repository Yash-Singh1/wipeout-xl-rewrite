package com.example.wipeoutxl;

import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;

import org.libsdl.app.SDLActivity;

/* SDL's activity, with the game (libmain.so, SDL linked into it) and the name of a document the system's file
   picker handed over (platform/mobile.c copies it into the app's folder under that name). */
public class WipeoutActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }

    public String displayName(String uri) {
        try (Cursor cursor = getContentResolver().query(Uri.parse(uri),
                new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                return cursor.getString(0);
            }
        } catch (Exception e) {
            // (no name: the caller makes one up)
        }
        return null;
    }
}
