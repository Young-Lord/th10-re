//@category TH10/Exports

import ghidra.app.analyzers.RelocationTableSynthesizerAnalyzer;
import ghidra.app.script.GhidraScript;
import ghidra.app.services.Analyzer;
import ghidra.app.util.DomainObjectService;
import ghidra.app.util.Option;
import ghidra.app.util.exporter.CoffRelocatableObjectExporter;
import ghidra.app.util.importer.MessageLog;
import ghidra.framework.model.DomainObject;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SourceType;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

// Exports object groups declared in config/ghidra_ns_to_obj.csv.
public class ExportTh10Delinker extends GhidraScript {
    private boolean createdExplicitFunction;

    private File[] getInputFiles() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 2) {
            return new File[] { new File(args[0]), new File(args[1]) };
        }
        if (args.length != 0) {
            throw new IllegalArgumentException(
                "Usage: ExportTh10Delinker.java <mapping.csv> <output-directory>");
        }
        return new File[] {
            askFile("TH10 object mapping", "Select"),
            askDirectory("TH10 object output", "Select")
        };
    }

    private Namespace findNamespace(String qualifiedName) {
        List<String> parts = new ArrayList<>(Arrays.asList(qualifiedName.split("::")));
        String symbolName = parts.remove(parts.size() - 1);
        Namespace current = null;
        for (String part : parts) {
            current = getNamespace(current, part);
            if (current == null) {
                return null;
            }
        }
        Symbol symbol = getSymbol(symbolName, current);
        return symbol != null && symbol.getObject() instanceof Namespace
            ? (Namespace) symbol.getObject()
            : null;
    }

    private Function findFunction(String addressOrName) throws Exception {
        if (addressOrName.startsWith("@")) {
            String addressSpec = addressOrName.substring(1);
            int rangeSeparator = addressSpec.indexOf('-');
            if (rangeSeparator >= 0) {
                Address address = toAddr(addressSpec.substring(0, rangeSeparator));
                Address endExclusive = toAddr(addressSpec.substring(rangeSeparator + 1));
                Function existing = currentProgram.getFunctionManager().getFunctionAt(address);
                if (existing != null) {
                    currentProgram.getFunctionManager().removeFunction(address);
                }
                AddressSet body = new AddressSet(address, endExclusive.subtract(1));
                createdExplicitFunction = true;
                return currentProgram.getFunctionManager().createFunction(
                    null, null, address, body, SourceType.USER_DEFINED);
            }
            Address address = toAddr(addressSpec);
            Function function = currentProgram.getFunctionManager().getFunctionAt(address);
            // Chain callbacks can be valid code labels that auto-analysis did not promote.
            // Define only explicit address mappings; names still require existing analysis symbols.
            if (function != null) {
                return function;
            }
            createdExplicitFunction = true;
            return createFunction(address, null);
        }
        for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
            if (function.getName().equals(addressOrName) ||
                function.getName(true).equals(addressOrName)) {
                return function;
            }
        }
        return null;
    }

    @Override
    protected void run() throws Exception {
        File[] inputFiles = getInputFiles();
        File mappingFile = inputFiles[0];
        File outputDirectory = inputFiles[1];
        if (!mappingFile.isFile()) {
            throw new IllegalArgumentException("Mapping file does not exist: " + mappingFile);
        }
        if (!outputDirectory.isDirectory() && !outputDirectory.mkdirs()) {
            throw new IllegalArgumentException("Cannot create output directory: " + outputDirectory);
        }

        Analyzer analyzer = new RelocationTableSynthesizerAnalyzer();
        analyzer.added(currentProgram, currentProgram.getMemory(), monitor, new MessageLog());

        CoffRelocatableObjectExporter exporter = new CoffRelocatableObjectExporter();
        List<Option> options = exporter.getOptions(new DomainObjectService() {
            @Override public DomainObject getDomainObject() { return currentProgram; }
        });
        Object noLeadingUnderscore = options.get(1).getValueClass().getEnumConstants()[0];
        options.set(1, new Option("Leading underscore", noLeadingUnderscore));
        exporter.setOptions(options);

        int exported = 0;
        for (String line : Files.readAllLines(mappingFile.toPath(), StandardCharsets.UTF_8)) {
            if (line.isBlank() || line.startsWith("#") || line.startsWith("object,")) {
                continue;
            }
            List<String> fields = new ArrayList<>(Arrays.asList(line.split(",")));
            if (fields.size() < 2) {
                throw new IllegalArgumentException("Invalid object mapping: " + line);
            }
            String objectName = fields.remove(0);
            AddressSet addressSet = new AddressSet();
            createdExplicitFunction = false;
            for (String mappingName : fields) {
                mappingName = mappingName.trim();
                Function function = findFunction(mappingName);
                if (function != null) {
                    addressSet = addressSet.union(function.getBody());
                    continue;
                }
                Namespace namespace = findNamespace(mappingName);
                if (namespace == null) {
                    printerr("Missing namespace or function: " + mappingName);
                    continue;
                }
                addressSet = addressSet.union(namespace.getBody());
            }
            if (addressSet.isEmpty()) {
                printerr("No address range for " + objectName + "; not exporting");
                continue;
            }
            if (createdExplicitFunction) {
                // Newly created callback bodies need a relocation pass before COFF export.
                analyzer.added(currentProgram, currentProgram.getMemory(), monitor, new MessageLog());
            }
            File outputFile = new File(outputDirectory, objectName + ".obj");
            if (!exporter.export(outputFile, currentProgram, addressSet, monitor)) {
                throw new IllegalStateException("COFF export failed: " + outputFile);
            }
            printf("Exported %s\n", outputFile);
            exported++;
        }
        if (exported == 0) {
            throw new IllegalStateException("No TH10 object groups were exported");
        }
    }
}
