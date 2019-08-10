/**
 * Defines how the host (editor) should sync document changes to the language server.
 */
[CCode (default_value = "LANGUAGE_SERVER_TEXT_DOCUMENT_SYNC_KIND_Unset")]
enum LanguageServer.TextDocumentSyncKind {
	Unset = -1,
	/**
	 * Documents should not be synced at all.
	 */
	None = 0,
	/**
	 * Documents are synced by always sending the full content of the document.
	 */
	Full = 1,
	/**
	 * Documents are synced by sending the full content on open. After that only incremental
	 * updates to the document are sent.
	 */
	Incremental = 2
}

enum LanguageServer.DiagnosticSeverity {
	Unset = 0,
	/**
	 * Reports an error.
	 */
	Error = 1,
	/**
	 * Reports a warning.
	 */
	Warning = 2,
	/**
	 * Reports an information.
	 */
	Information = 3,
	/**
	 * Reports a hint.
	 */
	Hint = 4
}

class LanguageServer.Position : Object {
	/**
	 * Line position in a document (zero-based).
	 */
	public uint line { get; set; default = -1; }

	/**
	 * Character offset on a line in a document (zero-based). Assuming that the line is
	 * represented as a string, the `character` value represents the gap between the
	 * `character` and `character + 1`.
	 *
	 * If the character value is greater than the line length it defaults back to the
	 * line length.
	 */
	public uint character { get; set; default = -1; }

        public int compare(Position other) {
            return line > other.line ? 1 : 
                (line == other.line ? 
                    (character > other.character ? 1 : 
                        (character == other.character ? 0 : -1)) : -1);
        }

        public string to_string() { return @"$line:$character"; }

	public LanguageServer.Position to_libvala () {
		return new Position () {
			line = this.line + 1,
			character = this.character
		};
	}

        public Position.from_libvala (Vala.SourceLocation sloc) {
            line = sloc.line - 1;
            character = sloc.column;
        }
}

class LanguageServer.Range : Object, Gee.Hashable<Range> {
	/**
	 * The range's start position.
	 */
	public Position start { get; set; }

	/**
	 * The range's end position.
	 */
	public Position end { get; set; }

        public string to_string() { return @"$start -> $end"; }

        public Range.from_sourceref (Vala.SourceReference sref) {
            this.start = new Position.from_libvala (sref.begin);
            this.end = new Position.from_libvala (sref.end);
        }

        public uint hash() { 
            debug ("computing hash");
            return this.to_string().hash();
        }

        public bool equal_to(Range other) { return this.to_string() == other.to_string(); }
}

class LanguageServer.Diagnostic : Object {
	/**
	 * The range at which the message applies.
	 */
	public Range range { get; set; }

	/**
	 * The diagnostic's severity. Can be omitted. If omitted it is up to the
	 * client to interpret diagnostics as error, warning, info or hint.
	 */
	public DiagnosticSeverity severity { get; set; }

	/**
	 * The diagnostic's code. Can be omitted.
	 */
	public string? code { get; set; }

	/**
	 * A human-readable string describing the source of this
	 * diagnostic, e.g. 'typescript' or 'super lint'.
	 */
	public string? source { get; set; }

	/**
	 * The diagnostic's message.
	 */
	public string message { get; set; }
}

/**
 * An event describing a change to a text document. If range and rangeLength are omitted
 * the new text is considered to be the full content of the document.
 */
class LanguageServer.TextDocumentContentChangeEvent : Object {
	public Range? range 		{ get; set; }
	public int rangeLength 	{ get; set; }
	public string text 			{ get; set; }
}

enum LanguageServer.MessageType {
	/**
	 * An error message.
	 */
	Error = 1,
	/**
	 * A warning message.
	 */
	Warning = 2,
	/**
	 * An information message.
	 */
	Info = 3,
	/**
	 * A log message.
	 */
	Log = 4
}

class LanguageServer.TextDocumentIdentifier : Object {
	public string uri { get; set; }
}

class LanguageServer.TextDocumentPositionParams : Object {
	public TextDocumentIdentifier textDocument { get; set; }
	public Position position { get; set; }
}

class LanguageServer.Location : Object {
	public string uri { get; set; }
	public Range range { get; set; }
}

class LanguageServer.DocumentSymbolParams: Object {
    public TextDocumentIdentifier textDocument { get; set; }
}

class LanguageServer.SymbolInformation : Object {
    public string name { get; set; }
    public SymbolKind kind { get; set; }
    public Location location { get; set; }
    public string? containerName { get; set; }
}

[CCode (default_value = "LANGUAGE_SERVER_SYMBOL_KIND_Variable")]
enum LanguageServer.SymbolKind {
	File = 1,
	Module = 2,
	Namespace = 3,
	Package = 4,
	Class = 5,
	Method = 6,
	Property = 7,
	Field = 8,
	Constructor = 9,
	Enum = 10,
	Interface = 11,
	Function = 12,
	Variable = 13,
	Constant = 14,
	String = 15,
	Number = 16,
	Boolean = 17,
	Array = 18,
}