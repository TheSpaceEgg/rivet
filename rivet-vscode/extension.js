const vscode = require('vscode');

function activate(context) {
    // 1. CUSTOMIZE COLORS HERE
    // Sequence: Pink, Orange, Blue, Green, Yellow, Red, Cyan
    // Opacity is set to 0.15 (15%) for a "highlight" look
    const colorDefinitions = [
        "rgba(255, 105, 180, 0.15)", // Pink
        "rgba(255, 165, 0, 0.15)",   // Orange
        "rgba(0, 100, 255, 0.15)",   // Blue
        "rgba(0, 255, 0, 0.15)",     // Green
        "rgba(255, 255, 0, 0.15)",   // Yellow
        "rgba(255, 0, 0, 0.15)",     // Red
        "rgba(0, 255, 255, 0.15)"    // Cyan
    ];

    const decorationTypes = colorDefinitions.map(color => 
        vscode.window.createTextEditorDecorationType({ backgroundColor: color })
    );

    let activeEditor = vscode.window.activeTextEditor;

    if (activeEditor) triggerUpdateDecorations();

    vscode.window.onDidChangeActiveTextEditor(editor => {
        activeEditor = editor;
        if (editor) triggerUpdateDecorations();
    }, null, context.subscriptions);

    vscode.workspace.onDidChangeTextDocument(event => {
        if (activeEditor && event.document === activeEditor.document) {
            triggerUpdateDecorations();
        }
    }, null, context.subscriptions);

    var timeout = null;
    function triggerUpdateDecorations() {
        if (timeout) clearTimeout(timeout);
        timeout = setTimeout(updateDecorations, 50);
    }

    function updateDecorations() {
        if (!activeEditor) return;
        
        // Ensure this only runs for your .rv files
        if (activeEditor.document.languageId !== 'rivet') return;

        const regEx = /^[\t ]+/gm;
        const text = activeEditor.document.getText();
        
        let tabSize = 4;
        if (typeof activeEditor.options.tabSize === 'number') {
            tabSize = activeEditor.options.tabSize;
        }

        const decorators = decorationTypes.map(() => []);

        let match;
        while (match = regEx.exec(text)) {
            const matchText = match[0];
            const matchLen = matchText.length;
            const startIndex = match.index;

            let depth = 0;
            for (let i = 0; i < matchLen; i += tabSize) {
                const colorIndex = depth % decorationTypes.length;
                const startPos = activeEditor.document.positionAt(startIndex + i);
                
                // Ensure we don't paint past the actual indent characters
                const endPos = activeEditor.document.positionAt(Math.min(startIndex + i + tabSize, startIndex + matchLen));

                decorators[colorIndex].push({ range: new vscode.Range(startPos, endPos) });
                depth++;
            }
        }

        decorationTypes.forEach((decorationType, index) => {
            activeEditor.setDecorations(decorationType, decorators[index]);
        });
    }
}

function deactivate() {}

module.exports = {
    activate,
    deactivate
}