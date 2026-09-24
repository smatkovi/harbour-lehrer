// Die Oberfläche auf Englisch.
//
// Der Kurs selbst trägt seine Sprachen im JSON; die Knöpfe und
// Überschriften ringsum standen bis hierher fest auf Deutsch, und damit
// half einem bilingualen Kurs sein Englisch wenig: Der Text war
// englisch, „Nochmal versuchen" darunter nicht.
//
// Der Schlüssel ist der deutsche Satz selbst. Das hat einen Grund: Wer
// die QML liest, sieht weiter, was dort steht, statt eines Kürzels, das
// man erst nachschlagen muss. Fehlt ein Eintrag, bleibt das Deutsche
// stehen — eine fehlende Übersetzung ist ein Schönheitsfehler, kein
// leerer Knopf.
//
// Manche Einträge sind Bruchstücke, die im Code zusammengesetzt werden
// ("Aufgabe " + n). Das ist nicht schön, aber die Alternative wäre, jede
// Zusammensetzung umzubauen; die Reihenfolge ist in beiden Sprachen
// dieselbe.
.pragma library

var EN = {
    "von": "of",
    "erledigt": "done",
    "Erwartet:": "Expected:",
    // -- Start und Übersicht
    "Kurs": "Course",
    "Kapitel": "Chapters",
    "Lektionen": "Lessons",
    "Stufe": "Level",
    "Stufe ": "Level ",
    "· Stufe ": "· level ",
    "(bis Stufe ": "(up to level ",
    "Einstufung": "Placement",
    "Einstufungstest": "Placement test",
    "Einstufung beginnen": "Start placement",
    "Einstufung ansehen": "Review placement",
    "Einstufung: alle Antworten": "Placement: all answers",
    "Neu einstufen": "Take placement again",
    "Test beginnen": "Start test",
    "Karteikarten": "Flashcards",
    "Karteikarten (": "Flashcards (",
    "Spielwiese": "Playground",
    "Spielwiese: eigenen Code laufen lassen": "Playground: run your own code",
    "Durchsicht": "Review",
    "Weiterlernen": "Continue",
    "Dort weiterlernen": "Continue there",
    "Dort anfangen": "Start there",
    "Zur Übersicht": "To the overview",
    "Zurück": "Back",
    "Zurück zum Kurs": "Back to the course",
    "Weiter": "Next",
    "Weiter: ": "Next: ",
    "Überspringen": "Skip",
    "Deutsch": "Deutsch",
    "English": "English",

    // -- Lektion
    "Beispiel": "Example",
    "Ausgabe": "Output",
    "Ausführen": "Run",
    "Laufen": "Run",
    "Laufen lassen": "Run it",
    "Stopp": "Stop",
    "läuft …": "running …",
    "Lektion abschließen": "Finish lesson",
    "Lektion geschafft": "Lesson done",
    "Diese Lektion ist durch.": "This lesson is done.",
    "Zu den Aufgaben (": "To the exercises (",
    "Erwartete Ausgabe:\n": "Expected output:\n",
    "Erwartet: ": "Expected: ",
    "Deine Vorhersage": "Your prediction",
    "Was schreibt das Programm?": "What does the program print?",
    "Diese Sprache lässt sich hier nicht ausführen.":
        "This language cannot be run here.",
    "C++ hat auf diesem Gerät keinen Übersetzer — diese Lektion wird gelesen und vorhergesagt.":
        "There is no C++ compiler on this device — this lesson is read and predicted.",
    "C++ braucht einen Übersetzer. Er steht in den Jolla-Quellen: pkcon install gcc-c++ — danach laufen auch diese Lektionen.":
        "C++ needs a compiler. It is in the Jolla repositories: pkcon install gcc-c++ — after that these lessons run too.",
    "Für dieses Kapitel fehlt NumPy auf dem Gerät — die Lektion bleibt lesbar.":
        "NumPy is missing on this device — the chapter stays readable.",
    "Für dieses Kapitel fehlt NumPy. Es liegt im Chum-Repo: pkcon install python3-numpy — danach läuft auch dieses Beispiel.":
        "NumPy is missing for this chapter. It is in the Chum repository: pkcon install python3-numpy — after that this example runs too.",
    "Zeilen, die mit plot beginnen, werden gezeichnet: ":
        "Lines starting with plot are drawn: ",
    "Zeilen, die mit plot beginnen, werden gezeichnet: print('plot %f %f' % (t, x)).":
        "Lines starting with plot are drawn: print('plot %f %f' % (t, x)).",
    "print('plot %f %f' % (t, x)) ergibt eine Kurve, ":
        "print('plot %f %f' % (t, x)) gives one curve, ",
    "print('plot name %f %f' % ...) mehrere.":
        "print('plot name %f %f' % ...) several.",

    // -- Aufgaben
    "Aufgabe ": "Exercise ",
    "Frage ": "Question ",
    "Prüfen": "Check",
    "Als Antwort prüfen": "Check as answer",
    "Nächste Aufgabe": "Next exercise",
    "Nochmal versuchen": "Try again",
    "Lösung anzeigen": "Show solution",
    "Richtig": "Correct",
    "Richtig: ": "Correct: ",
    "Richtig wäre etwa ": "It should be about ",
    "Daneben": "Not quite",
    "Deine Antwort: ": "Your answer: ",
    "Zahl": "Number",
    "Lücke ": "Gap ",
    "fehlendes Stück ": "missing piece ",
    "Bausteine": "Pieces",
    "Deine Lösung – tippe eine Zeile an, um sie zurückzulegen":
        "Your answer – tap a line to put it back",
    "Deine Lösung — antippen legt eine Zeile zurück":
        "Your answer — tapping a line puts it back",
    "Antworten ansehen": "See the answers",
    "Antworten durchsehen": "Go through the answers",
    "Für jetzt durch": "Done for now",
    "Geschafft": "Done",

    // -- Karteikarten
    "Umdrehen": "Flip",
    "Gewusst": "Knew it",
    "Wusste ich nicht": "Did not know",
    "Wackelig: ": "Shaky: ",
    "Noch nicht": "Not yet",
    "Nächste Karte": "Next card",
    "morgen wieder": "again tomorrow",
    "wieder in ": "again in ",
    "Tagen": "days",
    "heute": "today",
    "fällig": "due",
    "fällig)": "due)",
    "offen · ": "open · ",
    "zur Wiederholung fällig": "due for review",
    "zur Auffrischung wieder.": "for a refresher.",
    "Auffrischung fällig – Wiederholen nach Abstand ":
        "Refresher due – spaced repetition ",
    "ist der halbe Lernerfolg.": "is half the battle.",
    "Für heute ist nichts mehr fällig. Die nächsten Karten ":
        "Nothing more is due today. The next cards ",
    "Die Begriffe dieser Lektion kommen in ein paar Tagen ":
        "The terms from this lesson come back in a few days ",
    "kommen von selbst wieder — in ein paar Tagen, je nachdem, ":
        "come back by themselves — in a few days, depending on ",
    "Wiederholung wieder — in ein paar Tagen, je nachdem, ":
        "for review — in a few days, depending on ",
    "wie es lief.": "how it went.",
    "wie sicher sie saßen.": "how well they stuck.",
    "Karten bearbeitet. Was du richtig hattest, kommt in ":
        "cards done. What you got right comes back in ",

    // -- Einstufung und Fortschritt
    "Etwa zehn Fragen, ein Fingertipp je Frage. ":
        "About ten questions, one tap each. ",
    "Danach geht der Kurs genau dort weiter, wo dein ":
        "The course then carries on exactly where your ",
    "Wissen aufhört -- nichts, was du schon kannst.":
        "knowledge stops — nothing you already know.",
    "Fragen richtig.": "questions right.",
    "Daran solltest du arbeiten": "Worth working on",
    "Woran es noch hakt": "What is still shaky",
    "Geplant": "Planned",
    "· fertig": "· done",
    "von ": "of ",
    "noch ": "still ",
    "mit: ": "with: ",
    " (Zeile ": " (line ",
    "Aufgaben auf Anhieb richtig. ": "exercises right first time. ",
    "Aufgaben auf Anhieb richtig. Die Lektion kommt zur ":
        "exercises right first time. The lesson comes back for ",
    "Lektion(en) sind zur ": "lesson(s) are due for ",
    "richtig. Weiter geht es ": "right. It carries on ",
    "Du liegst über dem, was bisher geschrieben ist ":
        "You are past what has been written so far ",
    "Du liegst über dem, was bisher geschrieben ist. ":
        "You are past what has been written so far. ",
    "). Die höheren Kapitel kommen noch – ":
        "). The higher chapters are still to come – ",
    "findest du hier das, was es schon gibt.":
        "here is what there is already.",
    "unten steht der Plan.": "the plan is below.",
    // -- Spielwiese: gedeutet oder uebersetzt
    "C läuft hier gedeutet: picoc liest deinen Text und tut, was dort steht. Kein Übersetzen, also geht es sofort los.":
        "C runs interpreted here: picoc reads your text and does what it says. Nothing is compiled, so it starts at once.",
    "C++ wird übersetzt: g++ macht erst Maschinencode daraus und bindet ihn, dann läuft er. Das kostet die Sekunden vor der Ausgabe.":
        "C++ is compiled: g++ first turns it into machine code and links it, then that runs. Those are the seconds before the output.",
    "Rust läuft hier gedeutet: rrun liest deinen Text und tut, was dort steht. Kein Übersetzen, also geht es sofort los.":
        "Rust runs interpreted here: rrun reads your text and does what it says. Nothing is compiled, so it starts at once.",
    "Python wird gedeutet: CPython liest deinen Text. Der Deuter selbst muss aber erst hochkommen, und das sind die paar Zehntel vor der Ausgabe.":
        "Python is interpreted: CPython reads your text. But the interpreter itself has to start up first, and that is the fraction of a second before the output.",
    "Was heißt gedeutet und übersetzt?":
        "What do interpreted and compiled mean?",
    "**Gedeutet** heißt: ein Programm liest deinen Text und tut Zeile für Zeile, was dort steht. Es gibt nichts zu übersetzen, also fängt es sofort an — dafür ist der Deuter beim Laufen die ganze Zeit dabei und kostet Zeit an jeder Zeile.\n\n**Übersetzt** heißt: ein Übersetzer macht aus deinem Text einmal Maschinencode, den der Prozessor unmittelbar ausführt. Die Arbeit fällt **vorher** an, dafür läuft das Ergebnis danach schnell.\n\nBei kurzen Programmen sieht man deshalb fast nur das Übersetzen und kaum das Laufen. Bei C++ kommt dazu, dass eine einzige Zeile wie `#include <iostream>` rund 37 000 Zeilen Schablonen hereinholt, die der Übersetzer jedes Mal neu liest — das ist der größte Teil der Wartezeit, nicht dein Programm.\n\nUnd das sagt nichts darüber, welche Sprache schnell ist: C ist hier nur deshalb sofort da, weil diese App einen kleinen C-Deuter mitbringt. Richtig übersetztes C läuft schneller als alles andere hier — man wartet nur vorher.":
        "**Interpreted** means: a program reads your text and does, line by line, what it says. There is nothing to compile, so it starts at once — but the interpreter stays there while it runs and costs time on every line.\n\n**Compiled** means: a compiler turns your text into machine code once, and the processor runs that directly. The work is done **beforehand**; afterwards the result runs fast.\n\nWith short programs you therefore see almost nothing but the compiling and hardly any of the running. With C++ there is more to it: a single line such as `#include <iostream>` pulls in about 37,000 lines of templates that the compiler reads afresh every time — that is the greater part of the wait, not your program.\n\nAnd none of this says which language is fast: C is instant here only because this app carries a small C interpreter. Properly compiled C runs faster than anything else here — you just wait for it first.",
};

function w(text, sprache) {
    if (sprache !== "en")
        return text;
    var t = EN[text];
    return t === undefined ? text : t;
}
