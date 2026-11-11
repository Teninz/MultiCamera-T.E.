package ru.codex.obslancamera;
import android.content.*;
import android.database.*;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import java.io.*;
/** Gives the system installer temporary read access to exactly one verified APK. */
public class UpdateProvider extends ContentProvider {
    static final String AUTHORITY="ru.codex.obslancamera.updates";
    static Uri uri(){return Uri.parse("content://"+AUTHORITY+"/update.apk");}
    static File file(Context c){return new File(c.getCacheDir(),"updates/update.apk");}
    private void validate(Uri uri){if(!uri().equals(uri))throw new IllegalArgumentException("Unknown update URI");}
    public boolean onCreate(){return true;}
    public String getType(Uri uri){validate(uri);return "application/vnd.android.package-archive";}
    public ParcelFileDescriptor openFile(Uri uri,String mode)throws FileNotFoundException{validate(uri);if(!"r".equals(mode))throw new FileNotFoundException("Read only");return ParcelFileDescriptor.open(file(getContext()),ParcelFileDescriptor.MODE_READ_ONLY);}
    public Cursor query(Uri uri,String[] projection,String selection,String[] args,String order){validate(uri);String[] columns=projection==null?new String[]{OpenableColumns.DISPLAY_NAME,OpenableColumns.SIZE}:projection;MatrixCursor cursor=new MatrixCursor(columns);Object[] row=new Object[columns.length];for(int i=0;i<row.length;i++)row[i]=OpenableColumns.DISPLAY_NAME.equals(columns[i])?ReleasePolicy.APK:OpenableColumns.SIZE.equals(columns[i])?file(getContext()).length():null;cursor.addRow(row);return cursor;}
    public Uri insert(Uri uri,ContentValues values){throw new UnsupportedOperationException();}
    public int delete(Uri uri,String selection,String[] args){throw new UnsupportedOperationException();}
    public int update(Uri uri,ContentValues values,String selection,String[] args){throw new UnsupportedOperationException();}
}
