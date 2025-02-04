package ee.ria.cdoc;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HexFormat;

public class CDocTool {
    private enum Action {
        INVALID,
        ENCRYPT,
        DECRYPT,
        LOCKS,
        TEST
    }

    private static HexFormat hex = HexFormat.of();

    public static void main(String[] args) {
        System.out.println("Starting app...");

        String library = "../../build/libcdoc/libcdoc_java.jnilib";
        Action action = Action.INVALID;
        ArrayList<String> files = new ArrayList<>();
        String label = null;
        String password = null;
        String out = "test.cdoc2";
    
        for (int i = 0; i < args.length; i++) {
            if (args[i].equals("test")) {
                action = Action.TEST;
            } else if (args[i].equals("encrypt")) {
                action = Action.ENCRYPT;
            } else if (args[i].equals("decrypt")) {
                action = Action.DECRYPT;
            } else if (args[i].equals("locks")) {
                action = Action.LOCKS;
            } else if (args[i].equals("--label")) {
                i += 1;
                if (i >= args.length) {
                    System.err.println("Invalid arguments");
                    System.exit(1);
                }
                label = args[i];
            } else if (args[i].equals("--library")) {
                i += 1;
                if (i >= args.length) {
                    System.err.println("Invalid arguments");
                    System.exit(1);
                }
                library = args[i];
            } else if (args[i].equals("--password")) {
                i += 1;
                if (i >= args.length) {
                    System.err.println("Invalid arguments");
                    System.exit(1);
                }
                password = args[i];
            } else if (!args[i].startsWith("--")) {
                files.add(args[i]);
            }
        }

        File lib = new File(library);
        System.load(lib.getAbsolutePath());
        System.out.println("Library loaded");

        switch (action) {
            case ENCRYPT:
                encrypt(out, label, password, files);
                break;
            case DECRYPT:
                decrypt(files.get(0), label, password);
                break;
            case LOCKS:
                locks(files.get(0));
                break;
            case TEST:
                test();
                break;
        }
    }

    static void decrypt(String file, String label, String password) {
        System.out.println("Decrypting file " + file);
        try {
            ToolConf conf = new ToolConf();
            DataBuffer buf = new DataBuffer();
            ToolCrypto crypto = new ToolCrypto();
            crypto.setPassword(password);

            IStreamSource src = new IStreamSource(new FileInputStream(file));

            CDocReader rdr = CDocReader.createReader(src, false, conf, crypto, null);
            System.out.format("Reader created (version %d)\n", rdr.getVersion());

            //rdr.testConfig(buf);
            System.err.format("Buffer out: %s\n", hex.formatHex(buf.getData()));

            LockVector locks = rdr.getLocks();
            for (int idx = 0; idx < locks.size(); idx++) {
                Lock lock = locks.get(idx);
                if (lock.getLabel().equals(label)) {
                    System.out.format("Found lock: %s\n", label);
                    byte[] fmk = rdr.getFMK(idx);
                    System.out.format("FMK is: %s\n", hex.formatHex(fmk));
                    System.out.format("Stored data is: %s\n", hex.formatHex(stored.getData()));
                    rdr.beginDecryption(fmk);
                    FileInfo fi = new FileInfo();
                    int result = rdr.nextFile(fi);
                    System.out.format("nextFile result: %d\n", result);
                    try {
                        while (result == CDoc.OK) {
                            System.out.format("File %s length %d\n", fi.getName(), fi.getSize());
                            File ofile = new File(fi.getName());
                            OutputStream ofs = new FileOutputStream(ofile.getName());
                            rdr.readFile(ofs);
                            result = rdr.nextFile(fi);
                        }
                    } catch (IOException exc) {
                        System.err.println("IO Exception: " + exc.getMessage());
                    }
                    rdr.finishDecryption();
                    return;
                }
            }
            System.err.format("No such lock: %s\n", label);
        } catch (CDocException exc) {
            // Caught CDoc exception
        } catch (IOException exc) {
            // Caught CDoc exception
        }
    }

    static void encrypt(String file, String label, String password, Collection<String> files) {
        System.out.println("Creating file " + file);
        ToolConf conf = new ToolConf();
        ToolCrypto crypto = new ToolCrypto();
        crypto.setPassword(password);
        CDocWriter wrtr = CDocWriter.createWriter(2, file, conf, crypto, null);
        long result = wrtr.beginEncryption();
        System.out.format("beginEncryption: %d\n", result);
        Recipient rcpt = Recipient.makeSymmetric(label, 65535);
        result = wrtr.addRecipient(rcpt);
        System.out.format("addRecipient: %d\n", result);
        try {
            for (String name : files) {
                System.out.format("Adding file %s\n", name);
                InputStream ifs = new FileInputStream(name);
                byte[] bytes = ifs.readAllBytes();
                result = wrtr.addFile(name, bytes.length);
                System.out.format("addFile: %d\n", result);
                result = wrtr.writeData(bytes);
                System.out.format("writeData: %d\n", result);
            }
        } catch (IOException exc) {
            System.err.println("IO Exception: " + exc.getMessage());
        }
        result = wrtr.finishEncryption();
        System.out.format("finishEncryption: %d\n", result);
    }

    static void locks(String path) {
        System.out.println("Parsing file " + path);
        CDocReader rdr = CDocReader.createReader(path, null, null, null);
        System.out.format("Reader created (version %d)\n", rdr.getVersion());
        LockVector locks = rdr.getLocks();
        for (int i = 0; i < locks.size(); i++) {
            Lock lock = locks.get(i);
            System.out.format("Lock %d\n", i);
            System.out.format("  label: %s\n", lock.getLabel());
            System.out.format("  type: %s\n", lock.getType());
        }
    }

    static void test() {
        System.err.println("Testing label generation");
        String label = Recipient.buildLabel(new String[] {"Alpha", "1", "Beta", "2", "Gamma", "3", "Delta"});
        System.err.format("Label: %s\n", label);
        java.util.Map<String,String> map = Recipient.parseLabel(label);
        for (String key : map.keySet()) {
            System.err.format("  %s:%s\n", key, map.get(key));
        }

        String path = "test.cdoc2";

        System.err.println("Creating ToolConf...");
        ToolConf conf = new ToolConf();
        System.err.println("Creating DataBuffer...");
        DataBuffer buf = new DataBuffer();
        byte[] bytes = {1, 2, 3};
        buf.setData(bytes);
        System.err.format("Buffer: %s\n", hex.formatHex(buf.getData()));

        System.err.println("Creating ToolNetwork");
        ToolNetwork network = new ToolNetwork();

        System.err.println("Creating reader: " + path);
        CDocReader rdr = CDocReader.createReader(path, conf, null, network);
        System.err.format("Reader created (version %d)\n", rdr.getVersion());

        rdr.testConfig(buf);
        System.err.format("Buffer out: %s\n", hex.formatHex(buf.getData()));
        System.err.println("Success");

        CertificateList certs = new CertificateList();
        rdr.testNetwork(certs);
        System.err.format("Num certs: %s\n", certs.size());
        for (int i = 0; i < certs.size(); i++) {
            byte[] cert = certs.getCertificate(i);
            System.err.format("  %s\n", hex.formatHex(cert));
        }
        System.err.println("Success");
    }

    private static class ToolConf extends Configuration  {

        public long test(DataBuffer dst) {
            System.err.println("ToolConf.test: Java subclass implementation");
            //System.err.println("CPtr is: " + dst.getCPtr());
            Object obj = (Object) dst;
            System.err.println("ToolConf:Class: " + obj.getClass());
            System.err.println("ToolConf:Buffer is: " + dst.getData());
            byte[] bytes = {4, 5, 6, 7, 8};
            System.err.format("ToolConf:Buffer in: %s\n", hex.formatHex(dst.getData()));
            dst.setData(bytes);
            System.err.format("ToolConf:Buffer out: %s\n", hex.formatHex(dst.getData()));
            return CDoc.OK;
        }
    }

    private static DataBuffer stored = null;

    private static class ToolCrypto extends CryptoBackend {
        private byte[] secret;
        
        void setPassword(String password) {
            this.secret = password.getBytes();
        }
    
        @Override
        public int getSecret(DataBuffer dst, int idx) {
            stored = dst;
            dst.setData(secret);
            return CDoc.OK;
        }
    }

    private static class ToolNetwork extends NetworkBackend {
        @Override
        public long test(CertificateList dst) {
            System.err.println("ToolNetwork.test: Java subclass implementation");
            System.err.format("dst: %s\n", dst);
            dst.addCertificate(new byte[] {1, 2, 3});
            dst.addCertificate(new byte[] {4, 5, 6, 7, 8});
            return CDoc.OK;
        }
    }
}
