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
    "Für dieses Kapitel fehlt NumPy auf dem Gerät — die Lektion bleibt lesbar.":
        "NumPy is missing on this device — the chapter stays readable.",
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
    "unten steht der Plan.": "the plan is below."
};

function w(text, sprache) {
    if (sprache !== "en")
        return text;
    var t = EN[text];
    return t === undefined ? text : t;
}
