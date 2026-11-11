package ru.codex.obslancamera;

import java.io.*;
import java.net.*;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/** OLC1: fresh nonces, AES-GCM records, no plaintext pairing key on the network. */
public final class Wire implements Closeable {
    private final Socket socket;
    private final DataOutputStream out;
    private final byte[] key;
    private long sequence=1;
    public volatile long writeStarted;
    public Wire(String host,int port,String token) throws Exception {
        if(!host.matches("(?:10\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}|192\\.168\\.\\d{1,3}\\.\\d{1,3}|172\\.(?:1[6-9]|2[0-9]|3[01])\\.\\d{1,3}\\.\\d{1,3}|127\\.0\\.0\\.1)"))throw new IOException("Укажите локальный IPv4-адрес ПК");
        String[] octets=host.split("\\.");for(String part:octets)if(Integer.parseInt(part)>255)throw new IOException("Неверный IP-адрес");
        if(!token.matches("[0-9a-f]{32}"))throw new IOException("Ключ должен содержать 32 символа: 0–9, a–f");
        socket=new Socket();
        try {
            socket.setTcpNoDelay(true);socket.setSendBufferSize(64*1024);socket.connect(new InetSocketAddress(host,port),3000);socket.setSoTimeout(5000);
            out=new DataOutputStream(new BufferedOutputStream(socket.getOutputStream(),32*1024));
            DataInputStream in=new DataInputStream(socket.getInputStream());
            byte[] clientNonce=new byte[16];new SecureRandom().nextBytes(clientNonce);
            out.writeBytes("OLC1");out.write(clientNonce);out.flush();
            byte[] serverNonce=new byte[16];in.readFully(serverNonce);
            byte[] material=new byte[48];for(int i=0;i<16;i++)material[i]=(byte)Integer.parseInt(token.substring(i*2,i*2+2),16);
            System.arraycopy(clientNonce,0,material,16,16);System.arraycopy(serverNonce,0,material,32,16);
            key=MessageDigest.getInstance("SHA-256").digest(material);Arrays.fill(material,(byte)0);
            byte[] hello=crypt(Cipher.ENCRYPT_MODE,1,0,new byte[]{0,'H','E','L','L','O'});
            out.writeInt(8+hello.length);out.writeLong(0);out.write(hello);out.flush();
            byte[] ack=new byte[18];in.readFully(ack);
            if(!Arrays.equals(crypt(Cipher.DECRYPT_MODE,2,0,ack),"OK".getBytes(StandardCharsets.US_ASCII)))throw new IOException("OBS не подтвердил ключ подключения");
        } catch(Exception e){try{socket.close();}catch(IOException ignored){}throw e;}
    }
    private byte[] crypt(int mode,int direction,long seq,byte[] data) throws Exception {
        byte[] nonce=ByteBuffer.allocate(12).put(new byte[]{'O','L','C',(byte)direction}).putLong(seq).array();
        Cipher aes=Cipher.getInstance("AES/GCM/NoPadding");aes.init(mode,new SecretKeySpec(key,"AES"),new GCMParameterSpec(128,nonce));return aes.doFinal(data);
    }
    public synchronized void send(int type,int rotation,long timestamp,byte[] encoded) throws Exception {
        if(encoded.length>4*1024*1024-64)throw new IOException("Слишком большой видеокадр");
        byte[] plain=ByteBuffer.allocate(11+encoded.length).put((byte)type).putShort((short)rotation).putLong(timestamp).put(encoded).array();
        byte[] cipher=crypt(Cipher.ENCRYPT_MODE,1,sequence,plain);
        writeStarted=System.nanoTime();
        try{out.writeInt(cipher.length+8);out.writeLong(sequence++);out.write(cipher);out.flush();}finally{writeStarted=0;}
    }
    public void close(){try{socket.close();}catch(IOException ignored){}Arrays.fill(key,(byte)0);}
}
