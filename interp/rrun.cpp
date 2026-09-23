// rrun -- ein Deuter fuer den Rust-Ausschnitt, den der Kurs lehrt.
//
// Warum ueberhaupt: Auf dem N950 gibt es keinen Rust-Uebersetzer und wird
// es keinen geben -- rustc mit LLVM sind Hunderte Megabyte, und schon das
// Uebersetzen eines Einzeilers dauert dort laenger als eine Lektion. Die
// C-Lektionen laufen mit picoc (crun), die C++-Lektionen mit dem g++ 4.4
// aus dem Zusatzpaket. Fuer Rust bleibt nur ein eigener Deuter.
//
// Was er kann, richtet sich streng nach dem, was der Kurs lehrt:
//
//   let / let mut, Typangaben, Schatten (shadowing)
//   i64, f64, bool, char-freier String und &str, Vec<T>, Strukturen
//   if/else als Anweisung UND als Ausdruck, while, loop, for i in 0..n
//   fn mit Parametern, Rueckgabe, frueher return
//   &, &mut, Besitzuebergabe (move), .clone()
//   println!/print! mit {} und {:.3}
//   match ueber Ganzzahlen, Wahrheitswerte und _
//
// Und -- der eigentliche Grund, warum sich der Aufwand lohnt -- er meldet
// die drei Fehler, um die es in Rust geht, mit den echten Fehlernummern:
//
//   E0382  Verwendung nach der Besitzuebergabe
//   E0384/E0596  Aenderung ohne mut
//   E0499/E0502  zwei veraenderliche Ausleihen zugleich
//
// Was er NICHT ist: ein Uebersetzer. Er prueft nicht statisch, sondern
// waehrend des Laufs, und er kennt keine Lebensdauern ueber
// Funktionsgrenzen hinweg. Das steht auch so im Kurs: wer hier ein
// Programm zum Laufen bringt, hat es noch nicht an rustc vorbei. Eine
// Luege waere das erst, wenn die App so taete, als sei sie rustc -- und
// genau deshalb sagt sie es in der ersten Lektion.
//
//   rrun datei.rs

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------
// Fehler
// ---------------------------------------------------------------------

// Die Meldung traegt Zeile und Spalte in der Form, die der Laeufer der App
// auseinandernimmt ("datei:zeile:spalte Text"), dazu die Fehlernummer aus
// rustc. Wer die Nummer einmal gesehen hat, findet zu ihr auch draussen
// eine Erklaerung.
struct Fehler {
    int zeile;
    int spalte;
    std::string text;
};

static void werfen(int zeile, int spalte, const std::string &text)
{
    Fehler f;
    f.zeile = zeile;
    f.spalte = spalte;
    f.text = text;
    throw f;
}

// ---------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------

enum class Marke {
    Ende, Name, Ganzzahl, Kommazahl, Text, Zeichen,
    Klammer,      // ( ) { } [ ]
    Zeichenfolge  // Operatoren und Satzzeichen
};

struct Wortmarke {
    Marke art = Marke::Ende;
    std::string text;
    long long ganz = 0;
    double komma = 0.0;
    int zeile = 1;
    int spalte = 1;
};

class Zerleger {
public:
    explicit Zerleger(const std::string &quelle) : q(quelle) {}

    std::vector<Wortmarke> alles()
    {
        std::vector<Wortmarke> aus;
        for (;;) {
            Wortmarke m = naechste();
            aus.push_back(m);
            if (m.art == Marke::Ende)
                break;
        }
        return aus;
    }

private:
    const std::string q;
    size_t i = 0;
    int zeile = 1;
    int spalte = 1;

    char jetzt() const { return i < q.size() ? q[i] : '\0'; }
    char danach(size_t n = 1) const { return i + n < q.size() ? q[i + n] : '\0'; }

    void vor()
    {
        if (i < q.size()) {
            if (q[i] == '\n') { zeile++; spalte = 1; }
            else { spalte++; }
            i++;
        }
    }

    void leerraum()
    {
        for (;;) {
            while (jetzt() == ' ' || jetzt() == '\t' || jetzt() == '\r' || jetzt() == '\n')
                vor();
            // Zeilenkommentar
            if (jetzt() == '/' && danach() == '/') {
                while (jetzt() && jetzt() != '\n')
                    vor();
                continue;
            }
            // Blockkommentar, auch verschachtelt -- Rust erlaubt das.
            if (jetzt() == '/' && danach() == '*') {
                int tiefe = 0;
                do {
                    if (jetzt() == '/' && danach() == '*') { tiefe++; vor(); vor(); }
                    else if (jetzt() == '*' && danach() == '/') { tiefe--; vor(); vor(); }
                    else if (!jetzt()) break;
                    else vor();
                } while (tiefe > 0);
                continue;
            }
            break;
        }
    }

    Wortmarke naechste()
    {
        leerraum();
        Wortmarke m;
        m.zeile = zeile;
        m.spalte = spalte;
        if (!jetzt()) {
            m.art = Marke::Ende;
            return m;
        }
        char c = jetzt();

        // Name oder Schluesselwort
        if (isalpha((unsigned char)c) || c == '_') {
            std::string s;
            while (isalnum((unsigned char)jetzt()) || jetzt() == '_') {
                s += jetzt();
                vor();
            }
            // Makros heissen mit Ausrufezeichen: println!
            if (jetzt() == '!' && (s == "println" || s == "print" || s == "vec" ||
                                   s == "panic" || s == "assert")) {
                s += '!';
                vor();
            }
            m.art = Marke::Name;
            m.text = s;
            return m;
        }

        // Zahl
        if (isdigit((unsigned char)c)) {
            std::string s;
            bool komma = false;
            while (isdigit((unsigned char)jetzt()) || jetzt() == '_') {
                if (jetzt() != '_') s += jetzt();
                vor();
            }
            // Ein Punkt gehoert nur dann zur Zahl, wenn eine Ziffer folgt --
            // sonst ist es der Methodenpunkt in 1.max(2) oder der Bereich
            // 0..n, und beides darf die Zahl nicht verschlucken.
            if (jetzt() == '.' && isdigit((unsigned char)danach())) {
                komma = true;
                s += '.';
                vor();
                while (isdigit((unsigned char)jetzt()) || jetzt() == '_') {
                    if (jetzt() != '_') s += jetzt();
                    vor();
                }
            }
            if (jetzt() == 'e' || jetzt() == 'E') {
                size_t n = 1;
                if (danach() == '+' || danach() == '-') n = 2;
                if (isdigit((unsigned char)danach(n))) {
                    komma = true;
                    s += jetzt(); vor();
                    if (jetzt() == '+' || jetzt() == '-') { s += jetzt(); vor(); }
                    while (isdigit((unsigned char)jetzt())) { s += jetzt(); vor(); }
                }
            }
            // Typangabe an der Zahl: 1i64, 2.5f64, 3usize
            if (isalpha((unsigned char)jetzt())) {
                std::string suffix;
                while (isalnum((unsigned char)jetzt())) { suffix += jetzt(); vor(); }
                if (suffix == "f64" || suffix == "f32")
                    komma = true;
            }
            if (komma) {
                m.art = Marke::Kommazahl;
                m.komma = atof(s.c_str());
            } else {
                m.art = Marke::Ganzzahl;
                m.ganz = atoll(s.c_str());
            }
            m.text = s;
            return m;
        }

        // Zeichenkette
        if (c == '"') {
            vor();
            std::string s;
            while (jetzt() && jetzt() != '"') {
                if (jetzt() == '\\') {
                    vor();
                    char e = jetzt();
                    if (e == 'n') s += '\n';
                    else if (e == 't') s += '\t';
                    else if (e == '\\') s += '\\';
                    else if (e == '"') s += '"';
                    else if (e == '{') s += '{';
                    else if (e == '}') s += '}';
                    else s += e;
                    vor();
                } else {
                    s += jetzt();
                    vor();
                }
            }
            if (!jetzt())
                werfen(m.zeile, m.spalte, "error: nicht beendete Zeichenkette");
            vor();
            m.art = Marke::Text;
            m.text = s;
            return m;
        }

        // Einzelzeichen 'a'
        if (c == '\'') {
            vor();
            std::string s;
            while (jetzt() && jetzt() != '\'') {
                if (jetzt() == '\\') {
                    vor();
                    char e = jetzt();
                    if (e == 'n') s += '\n';
                    else if (e == 't') s += '\t';
                    else s += e;
                    vor();
                } else {
                    s += jetzt();
                    vor();
                }
            }
            vor();
            m.art = Marke::Zeichen;
            m.text = s;
            return m;
        }

        if (strchr("(){}[]", c)) {
            m.art = Marke::Klammer;
            m.text = std::string(1, c);
            vor();
            return m;
        }

        // Operatoren, laengste Zeichenfolge zuerst.
        static const char *lang3[] = {"..=", "<<=", ">>=", 0};
        static const char *lang2[] = {"==", "!=", "<=", ">=", "&&", "||", "->", "=>",
                                      "::", "+=", "-=", "*=", "/=", "%=", "..", 0};
        for (int k = 0; lang3[k]; k++) {
            if (q.compare(i, 3, lang3[k]) == 0) {
                m.art = Marke::Zeichenfolge;
                m.text = lang3[k];
                vor(); vor(); vor();
                return m;
            }
        }
        for (int k = 0; lang2[k]; k++) {
            if (q.compare(i, 2, lang2[k]) == 0) {
                m.art = Marke::Zeichenfolge;
                m.text = lang2[k];
                vor(); vor();
                return m;
            }
        }
        m.art = Marke::Zeichenfolge;
        m.text = std::string(1, c);
        vor();
        return m;
    }
};

// ---------------------------------------------------------------------
// Baum
// ---------------------------------------------------------------------

enum class Art {
    // Ausdruecke
    Ganz, Komma, Wahrheit, Text, Name, Aufruf, Methode, Index, Feld,
    Einstellig, Zweistellig, Ausleihe, Stern, Block, Wenn, Umwandlung,
    VecMakro, Bereich, Vergleiche, Verbundbau, Zeichenkette,
    // Anweisungen
    Lass, Zuweisung, Ausdruck, Solange, Schleife, Fuer, Brich, Weiter,
    Rueckgabe, Drucke
};

struct Knoten;
typedef std::shared_ptr<Knoten> KnotenP;

struct Knoten {
    Art art = Art::Ganz;
    int zeile = 1;
    int spalte = 1;
    long long ganz = 0;
    double komma = 0.0;
    bool wahr = false;
    // Name, Operator, Formatvorlage, Feldname, Typangabe
    std::string text;
    std::string zusatz;
    bool veraenderlich = false;
    bool zeilenumbruch = true;   // println! gegen print!
    std::vector<KnotenP> kinder;
};

static KnotenP neu(Art a, int zeile, int spalte)
{
    KnotenP k(new Knoten);
    k->art = a;
    k->zeile = zeile;
    k->spalte = spalte;
    return k;
}

struct Parameter {
    std::string name;
    bool veraenderlich = false;   // mut x
    bool verweis = false;         // &x oder &mut x
    bool verweis_mut = false;
    std::string typ;
};

struct Funktion {
    std::string name;
    std::vector<Parameter> parameter;
    std::string rueckgabe;
    KnotenP rumpf;
};

struct Verbunddef {
    std::string name;
    std::vector<std::string> felder;
};

// ---------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------

class Leser {
public:
    Leser(const std::vector<Wortmarke> &marken) : m(marken) {}

    void programm(std::map<std::string, Funktion> &funktionen,
                  std::map<std::string, Verbunddef> &verbunde)
    {
        while (!ende()) {
            if (istName("fn")) {
                Funktion f = funktion();
                funktionen[f.name] = f;
            } else if (istName("struct")) {
                Verbunddef v = verbund();
                verbunde[v.name] = v;
            } else if (istName("use") || istName("mod")) {
                // use std::mem; -- wird gelesen und verworfen, damit
                // Beispiele aus Buechern unveraendert laufen.
                while (!ende() && !istZeichen(";"))
                    weiter();
                erwarteZeichen(";");
            } else if (istName("impl")) {
                werfen(jetzt().zeile, jetzt().spalte,
                       "error: `impl` kennt dieser Deuter nicht -- der Kurs "
                       "kommt ohne Methoden auf eigenen Typen aus");
            } else {
                werfen(jetzt().zeile, jetzt().spalte,
                       "error: hier wird `fn` oder `struct` erwartet, gefunden: `"
                       + jetzt().text + "`");
            }
        }
    }

private:
    const std::vector<Wortmarke> &m;
    size_t i = 0;

    const Wortmarke &jetzt() const { return m[i]; }
    const Wortmarke &danach(size_t n = 1) const
    {
        return i + n < m.size() ? m[i + n] : m[m.size() - 1];
    }
    bool ende() const { return jetzt().art == Marke::Ende; }
    void weiter() { if (!ende()) i++; }

    bool istName(const char *s) const
    {
        return jetzt().art == Marke::Name && jetzt().text == s;
    }
    bool istZeichen(const char *s) const
    {
        return (jetzt().art == Marke::Zeichenfolge || jetzt().art == Marke::Klammer)
               && jetzt().text == s;
    }
    bool nimmName(const char *s) { if (istName(s)) { weiter(); return true; } return false; }
    bool nimmZeichen(const char *s) { if (istZeichen(s)) { weiter(); return true; } return false; }

    void erwarteZeichen(const char *s)
    {
        if (!nimmZeichen(s))
            werfen(jetzt().zeile, jetzt().spalte,
                   std::string("error: hier fehlt `") + s + "`, gefunden: `"
                   + (ende() ? std::string("Dateiende") : jetzt().text) + "`");
    }

    std::string erwarteName()
    {
        if (jetzt().art != Marke::Name)
            werfen(jetzt().zeile, jetzt().spalte, "error: hier wird ein Name erwartet");
        std::string s = jetzt().text;
        weiter();
        return s;
    }

    // Eine Typangabe wird gelesen und als Zeichenkette behalten. Der Deuter
    // prueft Typen beim Rechnen, nicht beim Lesen -- aber die Angabe
    // entscheidet, ob aus `let x = 0` eine Ganzzahl oder eine Kommazahl
    // wird, und das ist in Rust ein Unterschied.
    std::string typ()
    {
        std::string s;
        if (nimmZeichen("&")) {
            s += "&";
            if (nimmName("mut")) s += "mut ";
        }
        if (istZeichen("(")) {          // () -- der Einheitstyp
            weiter();
            erwarteZeichen(")");
            return s + "()";
        }
        s += erwarteName();
        if (nimmZeichen("<")) {
            s += "<";
            s += typ();
            while (nimmZeichen(","))
                s += "," + typ();
            erwarteZeichen(">");
            s += ">";
        }
        if (nimmZeichen("::")) {
            s += "::" + typ();
        }
        return s;
    }

    Verbunddef verbund()
    {
        weiter();                       // struct
        Verbunddef v;
        v.name = erwarteName();
        erwarteZeichen("{");
        while (!istZeichen("}") && !ende()) {
            std::string feld = erwarteName();
            erwarteZeichen(":");
            typ();
            v.felder.push_back(feld);
            if (!nimmZeichen(","))
                break;
        }
        erwarteZeichen("}");
        return v;
    }

    Funktion funktion()
    {
        weiter();                       // fn
        Funktion f;
        f.name = erwarteName();
        erwarteZeichen("(");
        while (!istZeichen(")") && !ende()) {
            Parameter p;
            if (nimmName("mut"))
                p.veraenderlich = true;
            p.name = erwarteName();
            erwarteZeichen(":");
            p.typ = typ();
            if (p.typ.size() && p.typ[0] == '&') {
                p.verweis = true;
                p.verweis_mut = p.typ.compare(0, 5, "&mut ") == 0;
            }
            f.parameter.push_back(p);
            if (!nimmZeichen(","))
                break;
        }
        erwarteZeichen(")");
        if (nimmZeichen("->"))
            f.rueckgabe = typ();
        f.rumpf = block();
        return f;
    }

    KnotenP block()
    {
        int z = jetzt().zeile, s = jetzt().spalte;
        erwarteZeichen("{");
        KnotenP b = neu(Art::Block, z, s);
        while (!istZeichen("}") && !ende()) {
            KnotenP a = anweisung();
            if (a)
                b->kinder.push_back(a);
        }
        erwarteZeichen("}");
        return b;
    }

    // Anweisungen ------------------------------------------------------

    KnotenP anweisung()
    {
        int z = jetzt().zeile, s = jetzt().spalte;

        if (istName("let")) {
            weiter();
            KnotenP k = neu(Art::Lass, z, s);
            if (nimmName("mut"))
                k->veraenderlich = true;
            k->text = erwarteName();
            if (nimmZeichen(":"))
                k->zusatz = typ();
            erwarteZeichen("=");
            k->kinder.push_back(ausdruck());
            erwarteZeichen(";");
            return k;
        }

        if (istName("while")) {
            weiter();
            KnotenP k = neu(Art::Solange, z, s);
            k->kinder.push_back(ausdruck(true));
            k->kinder.push_back(block());
            return k;
        }

        if (istName("loop")) {
            weiter();
            KnotenP k = neu(Art::Schleife, z, s);
            k->kinder.push_back(block());
            return k;
        }

        if (istName("for")) {
            weiter();
            KnotenP k = neu(Art::Fuer, z, s);
            k->text = erwarteName();
            if (!nimmName("in"))
                werfen(jetzt().zeile, jetzt().spalte, "error: hier fehlt `in`");
            k->kinder.push_back(ausdruck(true));
            k->kinder.push_back(block());
            return k;
        }

        if (istName("break")) {
            weiter();
            KnotenP k = neu(Art::Brich, z, s);
            nimmZeichen(";");
            return k;
        }

        if (istName("continue")) {
            weiter();
            KnotenP k = neu(Art::Weiter, z, s);
            nimmZeichen(";");
            return k;
        }

        if (istName("return")) {
            weiter();
            KnotenP k = neu(Art::Rueckgabe, z, s);
            if (!istZeichen(";"))
                k->kinder.push_back(ausdruck());
            nimmZeichen(";");
            return k;
        }

        if (istName("fn") || istName("struct"))
            werfen(z, s, "error: `fn` und `struct` stehen ganz aussen, nicht in "
                         "einem Block");

        // Alles andere ist ein Ausdruck -- mit oder ohne Zuweisung.
        KnotenP e = ausdruck();
        static const char *zuweiser[] = {"=", "+=", "-=", "*=", "/=", "%=", 0};
        for (int k = 0; zuweiser[k]; k++) {
            if (istZeichen(zuweiser[k])) {
                std::string op = jetzt().text;
                weiter();
                KnotenP zu = neu(Art::Zuweisung, z, s);
                zu->text = op;
                zu->kinder.push_back(e);
                zu->kinder.push_back(ausdruck());
                erwarteZeichen(";");
                return zu;
            }
        }
        KnotenP a = neu(Art::Ausdruck, z, s);
        // Ein Semikolon wirft den Wert weg; ohne Semikolon ist der Ausdruck
        // der Wert des Blocks. Genau daran haengt in Rust, ob eine Funktion
        // etwas zurueckgibt.
        a->wahr = !nimmZeichen(";");
        a->kinder.push_back(e);
        return a;
    }

    // Ausdruecke -------------------------------------------------------
    //
    // `ohne_verbund` verhindert, dass in `while x < n {` die geschweifte
    // Klammer als Aufbau einer Struktur gelesen wird -- dieselbe
    // Sonderregel hat Rust selbst.

    KnotenP ausdruck(bool ohne_verbund = false) { return oder(ohne_verbund); }

    KnotenP zweistellig(Art a, const std::string &op, KnotenP links, KnotenP rechts)
    {
        KnotenP k = neu(a, links->zeile, links->spalte);
        k->text = op;
        k->kinder.push_back(links);
        k->kinder.push_back(rechts);
        return k;
    }

    KnotenP oder(bool ov)
    {
        KnotenP l = und(ov);
        while (istZeichen("||")) {
            weiter();
            l = zweistellig(Art::Zweistellig, "||", l, und(ov));
        }
        return l;
    }

    KnotenP und(bool ov)
    {
        KnotenP l = vergleich(ov);
        while (istZeichen("&&")) {
            weiter();
            l = zweistellig(Art::Zweistellig, "&&", l, vergleich(ov));
        }
        return l;
    }

    KnotenP vergleich(bool ov)
    {
        KnotenP l = bereich(ov);
        static const char *ops[] = {"==", "!=", "<=", ">=", "<", ">", 0};
        for (;;) {
            bool gefunden = false;
            for (int k = 0; ops[k]; k++) {
                if (istZeichen(ops[k])) {
                    std::string op = jetzt().text;
                    weiter();
                    l = zweistellig(Art::Zweistellig, op, l, bereich(ov));
                    gefunden = true;
                    break;
                }
            }
            if (!gefunden)
                break;
        }
        return l;
    }

    KnotenP bereich(bool ov)
    {
        KnotenP l = summe(ov);
        if (istZeichen("..") || istZeichen("..=")) {
            bool einschliesslich = jetzt().text == "..=";
            weiter();
            KnotenP k = neu(Art::Bereich, l->zeile, l->spalte);
            k->wahr = einschliesslich;
            k->kinder.push_back(l);
            k->kinder.push_back(summe(ov));
            return k;
        }
        return l;
    }

    KnotenP summe(bool ov)
    {
        KnotenP l = produkt(ov);
        while (istZeichen("+") || istZeichen("-")) {
            std::string op = jetzt().text;
            weiter();
            l = zweistellig(Art::Zweistellig, op, l, produkt(ov));
        }
        return l;
    }

    KnotenP produkt(bool ov)
    {
        KnotenP l = umwandlung(ov);
        while (istZeichen("*") || istZeichen("/") || istZeichen("%")) {
            std::string op = jetzt().text;
            weiter();
            l = zweistellig(Art::Zweistellig, op, l, umwandlung(ov));
        }
        return l;
    }

    KnotenP umwandlung(bool ov)
    {
        KnotenP l = einstellig(ov);
        while (istName("as")) {
            weiter();
            KnotenP k = neu(Art::Umwandlung, l->zeile, l->spalte);
            k->text = typ();
            k->kinder.push_back(l);
            l = k;
        }
        return l;
    }

    KnotenP einstellig(bool ov)
    {
        int z = jetzt().zeile, s = jetzt().spalte;
        if (istZeichen("-") || istZeichen("!")) {
            std::string op = jetzt().text;
            weiter();
            KnotenP k = neu(Art::Einstellig, z, s);
            k->text = op;
            k->kinder.push_back(einstellig(ov));
            return k;
        }
        if (istZeichen("&")) {
            weiter();
            KnotenP k = neu(Art::Ausleihe, z, s);
            k->veraenderlich = nimmName("mut");
            k->kinder.push_back(einstellig(ov));
            return k;
        }
        if (istZeichen("*")) {
            weiter();
            KnotenP k = neu(Art::Stern, z, s);
            k->kinder.push_back(einstellig(ov));
            return k;
        }
        return nachgestellt(ov);
    }

    KnotenP nachgestellt(bool ov)
    {
        KnotenP e = grundwert(ov);
        for (;;) {
            int z = jetzt().zeile, s = jetzt().spalte;
            if (istZeichen(".")) {
                weiter();
                std::string name = erwarteName();
                if (istZeichen("(")) {
                    weiter();
                    KnotenP k = neu(Art::Methode, z, s);
                    k->text = name;
                    k->kinder.push_back(e);
                    while (!istZeichen(")") && !ende()) {
                        k->kinder.push_back(ausdruck());
                        if (!nimmZeichen(","))
                            break;
                    }
                    erwarteZeichen(")");
                    e = k;
                } else {
                    KnotenP k = neu(Art::Feld, z, s);
                    k->text = name;
                    k->kinder.push_back(e);
                    e = k;
                }
                continue;
            }
            if (istZeichen("[")) {
                weiter();
                KnotenP k = neu(Art::Index, z, s);
                k->kinder.push_back(e);
                k->kinder.push_back(ausdruck());
                erwarteZeichen("]");
                e = k;
                continue;
            }
            if (istZeichen("(") && e->art == Art::Name) {
                weiter();
                KnotenP k = neu(Art::Aufruf, e->zeile, e->spalte);
                k->text = e->text;
                while (!istZeichen(")") && !ende()) {
                    k->kinder.push_back(ausdruck());
                    if (!nimmZeichen(","))
                        break;
                }
                erwarteZeichen(")");
                e = k;
                continue;
            }
            break;
        }
        return e;
    }

    KnotenP grundwert(bool ov)
    {
        int z = jetzt().zeile, s = jetzt().spalte;

        if (jetzt().art == Marke::Ganzzahl) {
            KnotenP k = neu(Art::Ganz, z, s);
            k->ganz = jetzt().ganz;
            weiter();
            return k;
        }
        if (jetzt().art == Marke::Kommazahl) {
            KnotenP k = neu(Art::Komma, z, s);
            k->komma = jetzt().komma;
            weiter();
            return k;
        }
        if (jetzt().art == Marke::Text || jetzt().art == Marke::Zeichen) {
            KnotenP k = neu(Art::Zeichenkette, z, s);
            k->text = jetzt().text;
            weiter();
            return k;
        }
        if (istZeichen("(")) {
            weiter();
            if (istZeichen(")")) {       // der Einheitswert
                weiter();
                return neu(Art::Block, z, s);
            }
            KnotenP e = ausdruck();
            erwarteZeichen(")");
            return e;
        }
        if (istZeichen("{"))
            return block();

        if (istName("if"))
            return wenn();

        if (istName("match"))
            return vergleiche();

        if (istName("true") || istName("false")) {
            KnotenP k = neu(Art::Wahrheit, z, s);
            k->wahr = jetzt().text == "true";
            weiter();
            return k;
        }

        if (jetzt().art == Marke::Name) {
            std::string name = jetzt().text;

            if (name == "println!" || name == "print!") {
                weiter();
                KnotenP k = neu(Art::Drucke, z, s);
                k->zeilenumbruch = (name == "println!");
                erwarteZeichen("(");
                if (!istZeichen(")")) {
                    if (jetzt().art != Marke::Text)
                        werfen(jetzt().zeile, jetzt().spalte,
                               "error: das erste Argument von println! ist eine "
                               "Zeichenkette in Anfuehrungszeichen");
                    k->text = jetzt().text;
                    weiter();
                    while (nimmZeichen(",")) {
                        if (istZeichen(")"))
                            break;       // nachgestelltes Komma
                        k->kinder.push_back(ausdruck());
                    }
                }
                erwarteZeichen(")");
                return k;
            }

            if (name == "vec!") {
                weiter();
                KnotenP k = neu(Art::VecMakro, z, s);
                bool eckig = istZeichen("[");
                erwarteZeichen(eckig ? "[" : "(");
                if (!istZeichen(eckig ? "]" : ")")) {
                    k->kinder.push_back(ausdruck());
                    if (nimmZeichen(";")) {
                        k->wahr = true;             // vec![wert; anzahl]
                        k->kinder.push_back(ausdruck());
                    } else {
                        while (nimmZeichen(",")) {
                            if (istZeichen(eckig ? "]" : ")"))
                                break;
                            k->kinder.push_back(ausdruck());
                        }
                    }
                }
                erwarteZeichen(eckig ? "]" : ")");
                return k;
            }

            if (name == "panic!" || name == "assert!") {
                weiter();
                KnotenP k = neu(Art::Drucke, z, s);
                k->text = "__panic__";
                erwarteZeichen("(");
                while (!istZeichen(")") && !ende()) {
                    if (jetzt().art == Marke::Text) { weiter(); }
                    else k->kinder.push_back(ausdruck());
                    if (!nimmZeichen(","))
                        break;
                }
                erwarteZeichen(")");
                return k;
            }

            weiter();
            // Pfade wie String::from, Vec::new, std::mem::swap
            while (istZeichen("::")) {
                weiter();
                name += "::" + erwarteName();
            }

            // Aufbau einer Struktur: Punkt { x: 1.0, v: 0.0 }
            if (!ov && istZeichen("{") && !name.empty() && isupper((unsigned char)name[0])) {
                weiter();
                KnotenP k = neu(Art::Verbundbau, z, s);
                k->text = name;
                while (!istZeichen("}") && !ende()) {
                    std::string feld = erwarteName();
                    erwarteZeichen(":");
                    KnotenP wert = ausdruck();
                    wert->zusatz = feld;
                    k->kinder.push_back(wert);
                    if (!nimmZeichen(","))
                        break;
                }
                erwarteZeichen("}");
                return k;
            }

            KnotenP k = neu(Art::Name, z, s);
            k->text = name;
            return k;
        }

        werfen(z, s, "error: hier wird ein Wert erwartet, gefunden: `"
                     + (ende() ? std::string("Dateiende") : jetzt().text) + "`");
        return KnotenP();
    }

    KnotenP wenn()
    {
        int z = jetzt().zeile, s = jetzt().spalte;
        weiter();                        // if
        KnotenP k = neu(Art::Wenn, z, s);
        k->kinder.push_back(ausdruck(true));
        k->kinder.push_back(block());
        if (nimmName("else")) {
            if (istName("if"))
                k->kinder.push_back(wenn());
            else
                k->kinder.push_back(block());
        }
        return k;
    }

    KnotenP vergleiche()
    {
        int z = jetzt().zeile, s = jetzt().spalte;
        weiter();                        // match
        KnotenP k = neu(Art::Vergleiche, z, s);
        k->kinder.push_back(ausdruck(true));
        erwarteZeichen("{");
        while (!istZeichen("}") && !ende()) {
            // Muster: eine Zahl, ein Wahrheitswert, oder _
            KnotenP muster;
            if (istName("_")) {
                weiter();
                muster = neu(Art::Block, jetzt().zeile, jetzt().spalte);   // steht fuer "alles"
                muster->text = "_";
            } else {
                muster = ausdruck(true);
                // Mehrere Muster mit | -- 1 | 2 => ...
                while (istZeichen("|")) {
                    weiter();
                    KnotenP w = neu(Art::Zweistellig, muster->zeile, muster->spalte);
                    w->text = "|muster|";
                    w->kinder.push_back(muster);
                    w->kinder.push_back(ausdruck(true));
                    muster = w;
                }
            }
            if (!nimmZeichen("=>"))
                werfen(jetzt().zeile, jetzt().spalte, "error: hier fehlt `=>`");
            KnotenP koerper = istZeichen("{") ? block() : ausdruck();
            k->kinder.push_back(muster);
            k->kinder.push_back(koerper);
            nimmZeichen(",");
        }
        erwarteZeichen("}");
        return k;
    }
};

// ---------------------------------------------------------------------
// Werte, Zellen, Ausleihen
// ---------------------------------------------------------------------

enum class Wertart { Einheit, Ganz, Komma, Wahrheit, Text, Liste, Verbund, Verweis };

struct Wert;
struct Zelle;
typedef std::shared_ptr<Wert> WertP;
typedef std::shared_ptr<Zelle> ZelleP;

// Eine lebende Ausleihe. Sie zaehlt beim Entstehen hoch und beim
// Verschwinden wieder herunter -- der Zaehler haengt also an der
// Lebensdauer des Verweiswertes und nicht an einem Gueltigkeitsbereich.
// Das ist nahe an dem, was rustc seit der nicht-lexikalischen Lebensdauer
// tut: eine Ausleihe endet, sobald sie nicht mehr gebraucht wird.
struct Leihschein {
    ZelleP zelle;
    bool veraenderlich;
    ~Leihschein();
};
typedef std::shared_ptr<Leihschein> LeihscheinP;

struct Wert {
    Wertart art = Wertart::Einheit;
    long long ganz = 0;
    double komma = 0.0;
    bool wahr = false;
    std::string text;
    std::vector<WertP> liste;
    std::string verbundname;
    std::vector<std::pair<std::string, WertP> > felder;
    // Verweis
    ZelleP ziel;
    bool ziel_veraenderlich = false;
    LeihscheinP schein;
};

struct Zelle {
    WertP wert;
    std::string name;
    bool veraenderlich = false;
    bool verschoben = false;
    int geliehen = 0;
    int veraenderlich_geliehen = 0;
};

Leihschein::~Leihschein()
{
    if (!zelle)
        return;
    if (veraenderlich) {
        if (zelle->veraenderlich_geliehen > 0)
            zelle->veraenderlich_geliehen--;
    } else if (zelle->geliehen > 0) {
        zelle->geliehen--;
    }
}

static WertP einheit()
{
    WertP w(new Wert);
    w->art = Wertart::Einheit;
    return w;
}

static WertP ganzwert(long long v)
{
    WertP w(new Wert);
    w->art = Wertart::Ganz;
    w->ganz = v;
    return w;
}

static WertP kommawert(double v)
{
    WertP w(new Wert);
    w->art = Wertart::Komma;
    w->komma = v;
    return w;
}

static WertP wahrheitswert(bool v)
{
    WertP w(new Wert);
    w->art = Wertart::Wahrheit;
    w->wahr = v;
    return w;
}

static WertP textwert(const std::string &v)
{
    WertP w(new Wert);
    w->art = Wertart::Text;
    w->text = v;
    return w;
}

// Ganzzahlen, Kommazahlen und Wahrheitswerte sind in Rust `Copy`: eine
// Zuweisung kopiert sie und laesst das Original stehen. Alles andere --
// String, Vec, eigene Strukturen -- wird **verschoben**. Genau daran
// haengt der halbe Kurs.
static bool istKopierbar(const WertP &w)
{
    if (!w)
        return true;
    switch (w->art) {
    case Wertart::Ganz:
    case Wertart::Komma:
    case Wertart::Wahrheit:
    case Wertart::Einheit:
    case Wertart::Verweis:
        return true;
    default:
        return false;
    }
}

static WertP tiefekopie(const WertP &w)
{
    if (!w)
        return einheit();
    WertP n(new Wert(*w));
    n->liste.clear();
    for (size_t i = 0; i < w->liste.size(); i++)
        n->liste.push_back(tiefekopie(w->liste[i]));
    n->felder.clear();
    for (size_t i = 0; i < w->felder.size(); i++)
        n->felder.push_back(std::make_pair(w->felder[i].first,
                                           tiefekopie(w->felder[i].second)));
    return n;
}

static std::string typname(const WertP &w)
{
    if (!w)
        return "()";
    switch (w->art) {
    case Wertart::Ganz: return "i64";
    case Wertart::Komma: return "f64";
    case Wertart::Wahrheit: return "bool";
    case Wertart::Text: return "String";
    case Wertart::Liste: return "Vec";
    case Wertart::Verbund: return w->verbundname;
    case Wertart::Verweis: return "&";
    default: return "()";
    }
}

// ---------------------------------------------------------------------
// Umgebung
// ---------------------------------------------------------------------

class Umgebung {
public:
    explicit Umgebung(Umgebung *aeusseres = 0) : aussen(aeusseres) {}

    ZelleP finden(const std::string &name)
    {
        for (int i = (int)eintraege.size() - 1; i >= 0; i--)
            if (eintraege[i].first == name)
                return eintraege[i].second;
        // Funktionen haben ihre eigene Umgebung: was in main steht, sieht
        // eine andere Funktion nicht. Deshalb endet die Suche an der
        // Funktionsgrenze, nicht erst ganz aussen.
        if (aussen)
            return aussen->finden(name);
        return ZelleP();
    }

    ZelleP anlegen(const std::string &name, const WertP &wert, bool veraenderlich)
    {
        ZelleP z(new Zelle);
        z->wert = wert;
        z->name = name;
        z->veraenderlich = veraenderlich;
        eintraege.push_back(std::make_pair(name, z));
        return z;
    }

private:
    std::vector<std::pair<std::string, ZelleP> > eintraege;
    Umgebung *aussen;
};

// Steuerfluss ----------------------------------------------------------

struct Rueckkehr { WertP wert; };
struct Abbruch {};
struct Naechster {};

// ---------------------------------------------------------------------
// Deuter
// ---------------------------------------------------------------------

class Deuter {
public:
    std::map<std::string, Funktion> funktionen;
    std::map<std::string, Verbunddef> verbunde;

    void lauf()
    {
        std::map<std::string, Funktion>::iterator it = funktionen.find("main");
        if (it == funktionen.end())
            werfen(1, 1, "error[E0601]: `main` fehlt -- jedes Programm faengt dort an");
        Umgebung u(0);
        block(it->second.rumpf, u);
    }

    // -- Anweisungen und Bloecke --------------------------------------

    WertP block(const KnotenP &k, Umgebung &aussen)
    {
        Umgebung u(&aussen);
        WertP letzter = einheit();
        for (size_t i = 0; i < k->kinder.size(); i++)
            letzter = anweisung(k->kinder[i], u);
        return letzter;
    }

    WertP anweisung(const KnotenP &k, Umgebung &u)
    {
        switch (k->art) {
        case Art::Lass: {
            WertP w = wertVon(k->kinder[0], u, true);
            w = anpassen(w, k->zusatz, k);
            // Ein zweites `let` mit demselben Namen verdeckt das erste --
            // in Rust das uebliche Mittel, einen Wert umzuwandeln, ohne
            // einen zweiten Namen zu erfinden.
            u.anlegen(k->text, w, k->veraenderlich);
            return einheit();
        }
        case Art::Zuweisung:
            return zuweisen(k, u);
        case Art::Ausdruck: {
            WertP w = wertVon(k->kinder[0], u, false);
            return k->wahr ? w : einheit();
        }
        case Art::Solange: {
            for (;;) {
                WertP b = wertVon(k->kinder[0], u, false);
                if (b->art != Wertart::Wahrheit)
                    werfen(k->zeile, k->spalte,
                           "error[E0308]: die Bedingung von `while` muss `bool` sein, "
                           "hier steht `" + typname(b) + "` -- Rust hat keine "
                           "Wahrheit aus Zahlen");
                if (!b->wahr)
                    break;
                try {
                    block(k->kinder[1], u);
                } catch (Abbruch &) {
                    break;
                } catch (Naechster &) {
                    continue;
                }
            }
            return einheit();
        }
        case Art::Schleife: {
            for (;;) {
                try {
                    block(k->kinder[0], u);
                } catch (Abbruch &) {
                    break;
                } catch (Naechster &) {
                    continue;
                }
            }
            return einheit();
        }
        case Art::Fuer:
            return fuer(k, u);
        case Art::Brich:
            throw Abbruch();
        case Art::Weiter:
            throw Naechster();
        case Art::Rueckgabe: {
            Rueckkehr r;
            r.wert = k->kinder.empty() ? einheit() : wertVon(k->kinder[0], u, true);
            throw r;
        }
        default:
            return wertVon(k, u, false);
        }
    }

    WertP fuer(const KnotenP &k, Umgebung &u)
    {
        const KnotenP &quelle = k->kinder[0];
        if (quelle->art == Art::Bereich) {
            WertP a = wertVon(quelle->kinder[0], u, false);
            WertP b = wertVon(quelle->kinder[1], u, false);
            if (a->art != Wertart::Ganz || b->art != Wertart::Ganz)
                werfen(quelle->zeile, quelle->spalte,
                       "error[E0308]: ein Bereich `a..b` laeuft ueber Ganzzahlen; "
                       "fuer Kommazahlen zaehlt man ganzzahlig und rechnet den "
                       "Wert aus dem Index");
            long long ende = quelle->wahr ? b->ganz : b->ganz - 1;
            for (long long i = a->ganz; i <= ende; i++) {
                Umgebung schleife(&u);
                schleife.anlegen(k->text, ganzwert(i), false);
                try {
                    block(k->kinder[1], schleife);
                } catch (Abbruch &) {
                    break;
                } catch (Naechster &) {
                    continue;
                }
            }
            return einheit();
        }
        // for x in &v  /  for x in v.iter()
        WertP liste = wertVon(quelle, u, false);
        if (liste->art == Wertart::Verweis && liste->ziel)
            liste = liste->ziel->wert;
        if (liste->art != Wertart::Liste)
            werfen(quelle->zeile, quelle->spalte,
                   "error[E0277]: hierueber laesst sich nicht laufen -- ein "
                   "Bereich `0..n`, `&vec` oder `vec.iter()` geht");
        for (size_t i = 0; i < liste->liste.size(); i++) {
            Umgebung schleife(&u);
            schleife.anlegen(k->text, liste->liste[i], false);
            try {
                block(k->kinder[1], schleife);
            } catch (Abbruch &) {
                break;
            } catch (Naechster &) {
                continue;
            }
        }
        return einheit();
    }

    // -- Zuweisung ----------------------------------------------------

    WertP zuweisen(const KnotenP &k, Umgebung &u)
    {
        const KnotenP &ziel = k->kinder[0];
        WertP neuwert = wertVon(k->kinder[1], u, true);

        // Welche Zelle ist gemeint, und welcher Platz darin?
        WertP *platz = 0;
        ZelleP zelle;
        bool ueber_verweis = false;
        stelle(ziel, u, zelle, platz, ueber_verweis);

        // Schreibt man durch ein `&mut`, ist die Erlaubnis schon beim
        // Ausleihen geprueft worden -- und die Ausleihe, durch die man
        // schreibt, darf sich nicht selbst im Weg stehen.
        if (!ueber_verweis) {
            if (!zelle->veraenderlich && zelle->name.size())
                werfen(k->zeile, k->spalte,
                       "error[E0384]: `" + zelle->name + "` ist nicht veraenderlich -- "
                       "schreibe `let mut " + zelle->name + "`");
            if (zelle->geliehen > 0 || zelle->veraenderlich_geliehen > 0)
                werfen(k->zeile, k->spalte,
                       "error[E0506]: `" + zelle->name + "` ist gerade ausgeliehen und "
                       "darf solange nicht geaendert werden");
        }

        if (k->text != "=") {
            WertP alt = *platz;
            std::string op = k->text.substr(0, 1);
            neuwert = rechnen(op, alt, neuwert, k);
        }
        *platz = neuwert;
        zelle->verschoben = false;
        return einheit();
    }

    // Findet die Zelle und den Platz, in den geschrieben wird. `u[i] = x`
    // und `p.x = 1.0` gehen beide hierdurch -- die Regel, dass die
    // **Variable** veraenderlich sein muss, gilt auch fuer ihre Teile.
    void stelle(const KnotenP &k, Umgebung &u, ZelleP &zelle, WertP *&platz,
                bool &ueber_verweis)
    {
        if (k->art == Art::Name) {
            zelle = u.finden(k->text);
            if (!zelle)
                werfen(k->zeile, k->spalte,
                       "error[E0425]: `" + k->text + "` gibt es hier nicht");
            platz = &zelle->wert;
            return;
        }
        if (k->art == Art::Stern) {
            WertP v = wertVon(k->kinder[0], u, false);
            if (v->art != Wertart::Verweis)
                werfen(k->zeile, k->spalte,
                       "error[E0614]: nur ein Verweis laesst sich mit `*` aufloesen");
            if (!v->ziel_veraenderlich)
                werfen(k->zeile, k->spalte,
                       "error[E0594]: das ist eine gemeinsame Ausleihe `&` -- "
                       "schreiben darf nur `&mut`");
            zelle = v->ziel;
            platz = &zelle->wert;
            ueber_verweis = true;
            return;
        }
        if (k->art == Art::Index) {
            ZelleP z;
            WertP *p = 0;
            stelle(k->kinder[0], u, z, p, ueber_verweis);
            WertP liste = *p;
            if (liste->art == Wertart::Verweis && liste->ziel) {
                if (!liste->ziel_veraenderlich)
                    werfen(k->zeile, k->spalte,
                           "error[E0594]: das ist eine gemeinsame Ausleihe `&` -- "
                           "schreiben darf nur, wer `&mut` bekommen hat");
                z = liste->ziel;
                liste = z->wert;
                ueber_verweis = true;
            }
            if (liste->art != Wertart::Liste)
                werfen(k->zeile, k->spalte,
                       "error[E0608]: hier wird mit `[..]` zugegriffen, aber der "
                       "Wert ist kein `Vec`");
            WertP index = wertVon(k->kinder[1], u, false);
            if (index->art != Wertart::Ganz)
                werfen(k->zeile, k->spalte,
                       "error[E0308]: ein Index ist `usize`, hier steht `"
                       + typname(index) + "`");
            if (index->ganz < 0 || (size_t)index->ganz >= liste->liste.size()) {
                char puffer[160];
                snprintf(puffer, sizeof(puffer),
                         "panicked: index out of bounds: die Laenge ist %d, "
                         "der Index %lld", (int)liste->liste.size(), index->ganz);
                werfen(k->zeile, k->spalte, puffer);
            }
            zelle = z;
            platz = &liste->liste[(size_t)index->ganz];
            return;
        }
        if (k->art == Art::Feld) {
            ZelleP z;
            WertP *p = 0;
            stelle(k->kinder[0], u, z, p, ueber_verweis);
            WertP verbund = *p;
            if (verbund->art == Wertart::Verweis && verbund->ziel) {
                if (!verbund->ziel_veraenderlich)
                    werfen(k->zeile, k->spalte,
                           "error[E0594]: das ist eine gemeinsame Ausleihe `&` -- "
                           "schreiben darf nur, wer `&mut` bekommen hat");
                z = verbund->ziel;
                verbund = z->wert;
                ueber_verweis = true;
            }
            if (verbund->art != Wertart::Verbund)
                werfen(k->zeile, k->spalte,
                       "error[E0609]: `" + k->text + "` gibt es nur an einer Struktur");
            for (size_t i = 0; i < verbund->felder.size(); i++) {
                if (verbund->felder[i].first == k->text) {
                    zelle = z;
                    platz = &verbund->felder[i].second;
                    return;
                }
            }
            werfen(k->zeile, k->spalte,
                   "error[E0609]: die Struktur `" + verbund->verbundname
                   + "` hat kein Feld `" + k->text + "`");
        }
        werfen(k->zeile, k->spalte,
               "error[E0070]: hierhin laesst sich nichts zuweisen");
    }

    // -- Ausdruecke ---------------------------------------------------
    //
    // `verschiebend` sagt, ob der Wert weitergegeben wird (an ein `let`,
    // an einen Parameter, an ein `return`). Nur dann wird ein nicht
    // kopierbarer Wert aus seiner Variablen **herausgenommen**.

    WertP wertVon(const KnotenP &k, Umgebung &u, bool verschiebend)
    {
        switch (k->art) {
        case Art::Ganz: return ganzwert(k->ganz);
        case Art::Komma: return kommawert(k->komma);
        case Art::Wahrheit: return wahrheitswert(k->wahr);
        case Art::Zeichenkette: return textwert(k->text);
        case Art::Block: {
            if (k->text == "_")
                return einheit();
            return block(k, u);
        }
        case Art::Name: {
            ZelleP z = u.finden(k->text);
            if (!z)
                werfen(k->zeile, k->spalte,
                       "error[E0425]: `" + k->text + "` gibt es hier nicht");
            if (z->verschoben)
                werfen(k->zeile, k->spalte,
                       "error[E0382]: `" + k->text + "` wurde weitergegeben und "
                       "gehoert jetzt jemand anderem. Wer beides braucht, leiht "
                       "mit `&" + k->text + "` aus oder kopiert mit `"
                       + k->text + ".clone()`");
            if (verschiebend && !istKopierbar(z->wert)) {
                if (z->geliehen > 0 || z->veraenderlich_geliehen > 0)
                    werfen(k->zeile, k->spalte,
                           "error[E0505]: `" + k->text + "` ist ausgeliehen und "
                           "kann solange nicht weitergegeben werden");
                z->verschoben = true;
            }
            return z->wert;
        }
        case Art::Ausleihe: return ausleihen(k, u);
        case Art::Stern: {
            WertP v = wertVon(k->kinder[0], u, false);
            if (v->art != Wertart::Verweis)
                werfen(k->zeile, k->spalte,
                       "error[E0614]: nur ein Verweis laesst sich mit `*` aufloesen");
            return v->ziel ? v->ziel->wert : einheit();
        }
        case Art::Einstellig: {
            WertP a = wertVon(k->kinder[0], u, false);
            if (k->text == "-") {
                if (a->art == Wertart::Ganz) return ganzwert(-a->ganz);
                if (a->art == Wertart::Komma) return kommawert(-a->komma);
                werfen(k->zeile, k->spalte,
                       "error[E0600]: `-` geht nur auf Zahlen");
            }
            if (k->text == "!") {
                if (a->art == Wertart::Wahrheit) return wahrheitswert(!a->wahr);
                werfen(k->zeile, k->spalte,
                       "error[E0600]: `!` geht auf `bool`");
            }
            return einheit();
        }
        case Art::Zweistellig: return zweistelligWert(k, u);
        case Art::Umwandlung: {
            WertP a = wertVon(k->kinder[0], u, false);
            const std::string &t = k->text;
            if (t == "f64" || t == "f32") {
                if (a->art == Wertart::Ganz) return kommawert((double)a->ganz);
                if (a->art == Wertart::Komma) return a;
            }
            if (t == "i64" || t == "i32" || t == "usize" || t == "u64" || t == "u32") {
                if (a->art == Wertart::Komma) return ganzwert((long long)a->komma);
                if (a->art == Wertart::Ganz) return a;
            }
            werfen(k->zeile, k->spalte,
                   "error[E0605]: `as " + t + "` geht hier nicht");
            return einheit();
        }
        case Art::Wenn: {
            WertP b = wertVon(k->kinder[0], u, false);
            if (b->art != Wertart::Wahrheit)
                werfen(k->zeile, k->spalte,
                       "error[E0308]: die Bedingung von `if` muss `bool` sein, hier "
                       "steht `" + typname(b) + "`. Anders als in C gibt es keine "
                       "Wahrheit aus Zahlen -- schreibe den Vergleich hin");
            if (b->wahr)
                return block(k->kinder[1], u);
            if (k->kinder.size() > 2) {
                const KnotenP &sonst = k->kinder[2];
                return sonst->art == Art::Wenn ? wertVon(sonst, u, verschiebend)
                                               : block(sonst, u);
            }
            return einheit();
        }
        case Art::Vergleiche: return vergleichen(k, u);
        case Art::Index: {
            WertP liste = wertVon(k->kinder[0], u, false);
            if (liste->art == Wertart::Verweis && liste->ziel)
                liste = liste->ziel->wert;
            if (liste->art != Wertart::Liste)
                werfen(k->zeile, k->spalte,
                       "error[E0608]: hier wird mit `[..]` zugegriffen, aber der Wert "
                       "ist kein `Vec`");
            WertP index = wertVon(k->kinder[1], u, false);
            if (index->art != Wertart::Ganz)
                werfen(k->zeile, k->spalte,
                       "error[E0308]: ein Index ist `usize`, hier steht `"
                       + typname(index) + "` -- mit `as usize` umwandeln");
            if (index->ganz < 0 || (size_t)index->ganz >= liste->liste.size()) {
                char puffer[160];
                snprintf(puffer, sizeof(puffer),
                         "panicked: index out of bounds: die Laenge ist %d, der "
                         "Index %lld", (int)liste->liste.size(), index->ganz);
                werfen(k->zeile, k->spalte, puffer);
            }
            return liste->liste[(size_t)index->ganz];
        }
        case Art::Feld: {
            WertP v = wertVon(k->kinder[0], u, false);
            if (v->art == Wertart::Verweis && v->ziel)
                v = v->ziel->wert;
            if (v->art != Wertart::Verbund)
                werfen(k->zeile, k->spalte,
                       "error[E0609]: `." + k->text + "` gibt es nur an einer Struktur");
            for (size_t i = 0; i < v->felder.size(); i++)
                if (v->felder[i].first == k->text)
                    return v->felder[i].second;
            werfen(k->zeile, k->spalte,
                   "error[E0609]: die Struktur `" + v->verbundname + "` hat kein Feld `"
                   + k->text + "`");
            return einheit();
        }
        case Art::VecMakro: return vecmakro(k, u);
        case Art::Verbundbau: return verbundbau(k, u);
        case Art::Drucke: return drucken(k, u);
        case Art::Methode: return methode(k, u);
        case Art::Aufruf: return aufruf(k, u);
        case Art::Bereich:
            werfen(k->zeile, k->spalte,
                   "error: ein Bereich `a..b` steht hier nur in einem `for`");
            return einheit();
        default:
            return anweisung(k, u);
        }
    }

    WertP ausleihen(const KnotenP &k, Umgebung &u)
    {
        const KnotenP &innen = k->kinder[0];
        if (innen->art != Art::Name) {
            // &vec[0] und aehnliches braucht der Kurs nicht; ein Verweis auf
            // einen Zwischenwert waere ohnehin die naechste Falle.
            WertP w = wertVon(innen, u, false);
            WertP v(new Wert);
            v->art = Wertart::Verweis;
            v->ziel = ZelleP(new Zelle);
            v->ziel->wert = w;
            v->ziel->veraenderlich = k->veraenderlich;
            v->ziel_veraenderlich = k->veraenderlich;
            return v;
        }
        ZelleP z = u.finden(innen->text);
        if (!z)
            werfen(k->zeile, k->spalte,
                   "error[E0425]: `" + innen->text + "` gibt es hier nicht");
        if (z->verschoben)
            werfen(k->zeile, k->spalte,
                   "error[E0382]: `" + innen->text + "` wurde weitergegeben und "
                   "laesst sich nicht mehr ausleihen");
        if (k->veraenderlich) {
            if (!z->veraenderlich)
                werfen(k->zeile, k->spalte,
                       "error[E0596]: `" + innen->text + "` ist nicht veraenderlich "
                       "-- `&mut` gibt es nur auf `let mut`");
            if (z->veraenderlich_geliehen > 0)
                werfen(k->zeile, k->spalte,
                       "error[E0499]: `" + innen->text + "` ist schon veraenderlich "
                       "ausgeliehen. Zwei `&mut` auf dieselbe Sache gibt es nie -- "
                       "das ist die Regel, die Rust vor Datenrennen schuetzt");
            if (z->geliehen > 0)
                werfen(k->zeile, k->spalte,
                       "error[E0502]: `" + innen->text + "` ist bereits gemeinsam "
                       "ausgeliehen; solange ein `&` lebt, gibt es kein `&mut`");
            z->veraenderlich_geliehen++;
        } else {
            if (z->veraenderlich_geliehen > 0)
                werfen(k->zeile, k->spalte,
                       "error[E0502]: `" + innen->text + "` ist veraenderlich "
                       "ausgeliehen; solange gibt es kein zweites `&`");
            z->geliehen++;
        }
        WertP v(new Wert);
        v->art = Wertart::Verweis;
        v->ziel = z;
        v->ziel_veraenderlich = k->veraenderlich;
        v->schein = LeihscheinP(new Leihschein);
        v->schein->zelle = z;
        v->schein->veraenderlich = k->veraenderlich;
        return v;
    }

    // -- Rechnen ------------------------------------------------------

    WertP zweistelligWert(const KnotenP &k, Umgebung &u)
    {
        const std::string &op = k->text;
        if (op == "&&" || op == "||") {
            WertP a = wertVon(k->kinder[0], u, false);
            if (a->art != Wertart::Wahrheit)
                werfen(k->zeile, k->spalte,
                       "error[E0308]: `" + op + "` verlangt `bool`");
            // Kurzschluss wie in C: die rechte Seite wird nur bei Bedarf
            // ausgewertet.
            if (op == "&&" && !a->wahr) return wahrheitswert(false);
            if (op == "||" && a->wahr) return wahrheitswert(true);
            WertP b = wertVon(k->kinder[1], u, false);
            if (b->art != Wertart::Wahrheit)
                werfen(k->zeile, k->spalte,
                       "error[E0308]: `" + op + "` verlangt `bool`");
            return wahrheitswert(b->wahr);
        }
        WertP a = wertVon(k->kinder[0], u, false);
        WertP b = wertVon(k->kinder[1], u, false);
        return rechnen(op, a, b, k);
    }

    WertP rechnen(const std::string &op, WertP a, WertP b, const KnotenP &k)
    {
        if (a->art == Wertart::Verweis && a->ziel) a = a->ziel->wert;
        if (b->art == Wertart::Verweis && b->ziel) b = b->ziel->wert;

        // Der Unterschied zu C, an dem sich Umsteiger am haeufigsten
        // stossen: Rust wandelt **nichts** stillschweigend um. `1 / 2.0`
        // ist kein Rundungsfehler, sondern ein Uebersetzungsfehler.
        if ((a->art == Wertart::Ganz && b->art == Wertart::Komma) ||
            (a->art == Wertart::Komma && b->art == Wertart::Ganz))
            werfen(k->zeile, k->spalte,
                   "error[E0308]: `i64` und `f64` lassen sich nicht miteinander "
                   "verrechnen. Rust wandelt nie von selbst um -- schreibe `as f64` "
                   "an die Ganzzahl oder gleich `2.0` statt `2`");

        if (a->art == Wertart::Text && b->art == Wertart::Text && op == "+")
            return textwert(a->text + b->text);

        if (a->art == Wertart::Wahrheit && b->art == Wertart::Wahrheit) {
            if (op == "==") return wahrheitswert(a->wahr == b->wahr);
            if (op == "!=") return wahrheitswert(a->wahr != b->wahr);
        }
        if (a->art == Wertart::Text && b->art == Wertart::Text) {
            if (op == "==") return wahrheitswert(a->text == b->text);
            if (op == "!=") return wahrheitswert(a->text != b->text);
        }

        bool komma = a->art == Wertart::Komma;
        if (a->art != Wertart::Ganz && a->art != Wertart::Komma)
            werfen(k->zeile, k->spalte,
                   "error[E0369]: `" + op + "` geht nicht auf `" + typname(a) + "`");

        if (komma) {
            double x = a->komma, y = b->komma;
            if (op == "+") return kommawert(x + y);
            if (op == "-") return kommawert(x - y);
            if (op == "*") return kommawert(x * y);
            if (op == "/") return kommawert(x / y);
            if (op == "%") return kommawert(fmod(x, y));
            if (op == "==") return wahrheitswert(x == y);
            if (op == "!=") return wahrheitswert(x != y);
            if (op == "<") return wahrheitswert(x < y);
            if (op == "<=") return wahrheitswert(x <= y);
            if (op == ">") return wahrheitswert(x > y);
            if (op == ">=") return wahrheitswert(x >= y);
        } else {
            long long x = a->ganz, y = b->ganz;
            if ((op == "/" || op == "%") && y == 0)
                werfen(k->zeile, k->spalte,
                       "panicked: attempt to divide by zero -- in Rust ist die "
                       "Division durch null kein Zufallswert, sondern ein Abbruch");
            if (op == "+") return ganzwert(x + y);
            if (op == "-") return ganzwert(x - y);
            if (op == "*") return ganzwert(x * y);
            if (op == "/") return ganzwert(x / y);
            if (op == "%") return ganzwert(x % y);
            if (op == "==") return wahrheitswert(x == y);
            if (op == "!=") return wahrheitswert(x != y);
            if (op == "<") return wahrheitswert(x < y);
            if (op == "<=") return wahrheitswert(x <= y);
            if (op == ">") return wahrheitswert(x > y);
            if (op == ">=") return wahrheitswert(x >= y);
        }
        werfen(k->zeile, k->spalte, "error: unbekannter Operator `" + op + "`");
        return einheit();
    }

    // Eine Typangabe am `let` entscheidet, was aus einer blanken Zahl wird.
    // `let x: f64 = 0;` waere in Rust ein Fehler -- das sagen wir auch.
    WertP anpassen(WertP w, const std::string &typ, const KnotenP &k)
    {
        if (typ.empty() || !w)
            return w;
        bool will_komma = typ.find("f64") != std::string::npos ||
                          typ.find("f32") != std::string::npos;
        bool will_ganz = typ.find("i64") != std::string::npos ||
                         typ.find("i32") != std::string::npos ||
                         typ.find("usize") != std::string::npos ||
                         typ.find("u64") != std::string::npos;
        if (will_komma && w->art == Wertart::Ganz) {
            if (typ.find("Vec") != std::string::npos)
                return w;
            werfen(k->zeile, k->spalte,
                   "error[E0308]: hier steht `" + typ + "`, der Wert ist aber eine "
                   "Ganzzahl. In Rust ist `0` nicht `0.0` -- schreibe den Punkt hin");
        }
        if (will_ganz && w->art == Wertart::Komma)
            werfen(k->zeile, k->spalte,
                   "error[E0308]: hier steht `" + typ + "`, der Wert ist aber eine "
                   "Kommazahl");
        return w;
    }

    // -- match --------------------------------------------------------

    bool passt(const KnotenP &muster, const WertP &wert, Umgebung &u)
    {
        if (muster->art == Art::Block && muster->text == "_")
            return true;
        if (muster->art == Art::Zweistellig && muster->text == "|muster|")
            return passt(muster->kinder[0], wert, u) || passt(muster->kinder[1], wert, u);
        WertP m = wertVon(muster, u, false);
        if (m->art != wert->art)
            return false;
        switch (m->art) {
        case Wertart::Ganz: return m->ganz == wert->ganz;
        case Wertart::Komma: return m->komma == wert->komma;
        case Wertart::Wahrheit: return m->wahr == wert->wahr;
        case Wertart::Text: return m->text == wert->text;
        default: return false;
        }
    }

    WertP vergleichen(const KnotenP &k, Umgebung &u)
    {
        WertP wert = wertVon(k->kinder[0], u, false);
        for (size_t i = 1; i + 1 < k->kinder.size(); i += 2) {
            if (passt(k->kinder[i], wert, u)) {
                const KnotenP &koerper = k->kinder[i + 1];
                return koerper->art == Art::Block ? block(koerper, u)
                                                  : wertVon(koerper, u, true);
            }
        }
        werfen(k->zeile, k->spalte,
               "error[E0004]: `match` deckt nicht alle Faelle ab -- in Rust muss "
               "jeder moegliche Wert getroffen sein, notfalls mit `_`");
        return einheit();
    }

    // -- vec!, Strukturen ---------------------------------------------

    WertP vecmakro(const KnotenP &k, Umgebung &u)
    {
        WertP v(new Wert);
        v->art = Wertart::Liste;
        if (k->kinder.empty())
            return v;
        if (k->wahr) {                     // vec![wert; anzahl]
            WertP muster = wertVon(k->kinder[0], u, false);
            WertP anzahl = wertVon(k->kinder[1], u, false);
            if (anzahl->art != Wertart::Ganz)
                werfen(k->zeile, k->spalte,
                       "error[E0308]: die Anzahl in `vec![x; n]` ist eine Ganzzahl");
            for (long long i = 0; i < anzahl->ganz; i++)
                v->liste.push_back(tiefekopie(muster));
            return v;
        }
        for (size_t i = 0; i < k->kinder.size(); i++)
            v->liste.push_back(wertVon(k->kinder[i], u, true));
        return v;
    }

    WertP verbundbau(const KnotenP &k, Umgebung &u)
    {
        std::map<std::string, Verbunddef>::iterator it = verbunde.find(k->text);
        if (it == verbunde.end())
            werfen(k->zeile, k->spalte,
                   "error[E0422]: die Struktur `" + k->text + "` ist nicht erklaert");
        WertP v(new Wert);
        v->art = Wertart::Verbund;
        v->verbundname = k->text;
        for (size_t i = 0; i < k->kinder.size(); i++)
            v->felder.push_back(std::make_pair(k->kinder[i]->zusatz,
                                               wertVon(k->kinder[i], u, true)));
        if (v->felder.size() != it->second.felder.size())
            werfen(k->zeile, k->spalte,
                   "error[E0063]: der Struktur `" + k->text + "` fehlen Felder -- "
                   "in Rust wird jedes einzelne gesetzt");
        return v;
    }

    // -- Ausgabe ------------------------------------------------------

    std::string zeichenfolge(const WertP &w, const std::string &form, const KnotenP &k)
    {
        WertP v = w;
        if (v->art == Wertart::Verweis && v->ziel)
            v = v->ziel->wert;
        char puffer[256];
        // {:.3} -- feste Zahl von Nachkommastellen
        if (form.size() > 1 && form[0] == '.') {
            int stellen = atoi(form.c_str() + 1);
            if (v->art == Wertart::Komma) {
                snprintf(puffer, sizeof(puffer), "%.*f", stellen, v->komma);
                return puffer;
            }
            if (v->art == Wertart::Ganz)
                werfen(k->zeile, k->spalte,
                       "error[E0277]: `{:." + form.substr(1) + "}` gibt es nur fuer "
                       "Kommazahlen -- eine Ganzzahl hat keine Nachkommastellen");
        }
        if (form.size() && (form[form.size() - 1] == 'e' || form[form.size() - 1] == 'E')) {
            double x = v->art == Wertart::Komma ? v->komma : (double)v->ganz;
            snprintf(puffer, sizeof(puffer), "%e", x);
            return puffer;
        }
        switch (v->art) {
        case Wertart::Ganz:
            snprintf(puffer, sizeof(puffer), "%lld", v->ganz);
            return puffer;
        case Wertart::Komma: {
            // Rust schreibt eine Kommazahl ohne Nachkommastellen als "2",
            // wenn man {} nimmt? Nein -- es schreibt "2". Genauer: der
            // Debug-Druck haengt ein ".0" an, der Anzeigedruck nicht. Hier
            // wird der haeufigere Fall genommen: {} wie Rusts Display,
            // also die kuerzeste Form, die die Zahl zurueckliest.
            snprintf(puffer, sizeof(puffer), "%.17g", v->komma);
            std::string s = puffer;
            // 0.30000000000000004 ist richtig, 0.10000000000000001 nicht --
            // deshalb erst mit 15 Stellen versuchen und nur verlaengern,
            // wenn es nicht genau zurueckliest.
            snprintf(puffer, sizeof(puffer), "%.15g", v->komma);
            if (atof(puffer) == v->komma)
                s = puffer;
            else {
                snprintf(puffer, sizeof(puffer), "%.16g", v->komma);
                if (atof(puffer) == v->komma)
                    s = puffer;
            }
            return s;
        }
        case Wertart::Wahrheit: return v->wahr ? "true" : "false";
        case Wertart::Text: return v->text;
        case Wertart::Liste: {
            std::string s = "[";
            for (size_t i = 0; i < v->liste.size(); i++) {
                if (i) s += ", ";
                s += zeichenfolge(v->liste[i], form, k);
            }
            return s + "]";
        }
        case Wertart::Verbund: {
            std::string s = v->verbundname + " { ";
            for (size_t i = 0; i < v->felder.size(); i++) {
                if (i) s += ", ";
                s += v->felder[i].first + ": " + zeichenfolge(v->felder[i].second, form, k);
            }
            return s + " }";
        }
        default: return "()";
        }
    }

    WertP drucken(const KnotenP &k, Umgebung &u)
    {
        if (k->text == "__panic__")
            werfen(k->zeile, k->spalte, "panicked: der Lauf wurde abgebrochen");

        std::vector<WertP> werte;
        for (size_t i = 0; i < k->kinder.size(); i++)
            werte.push_back(wertVon(k->kinder[i], u, false));

        std::string aus;
        size_t naechster = 0;
        const std::string &v = k->text;
        for (size_t i = 0; i < v.size(); i++) {
            if (v[i] == '{' && i + 1 < v.size() && v[i + 1] == '{') { aus += '{'; i++; continue; }
            if (v[i] == '}' && i + 1 < v.size() && v[i + 1] == '}') { aus += '}'; i++; continue; }
            if (v[i] != '{') { aus += v[i]; continue; }
            size_t zu = v.find('}', i);
            if (zu == std::string::npos) {
                aus += v[i];
                continue;
            }
            std::string inhalt = v.substr(i + 1, zu - i - 1);
            std::string form;
            size_t doppel = inhalt.find(':');
            if (doppel != std::string::npos)
                form = inhalt.substr(doppel + 1);
            if (naechster >= werte.size())
                werfen(k->zeile, k->spalte,
                       "error: in der Vorlage stehen mehr `{}` als Werte dahinter");
            aus += zeichenfolge(werte[naechster], form, k);
            naechster++;
            i = zu;
        }
        if (naechster < werte.size())
            werfen(k->zeile, k->spalte,
                   "error: es stehen mehr Werte da, als die Vorlage `{}` hat");
        fputs(aus.c_str(), stdout);
        if (k->zeilenumbruch)
            fputc('\n', stdout);
        return einheit();
    }

    // -- Methoden und Funktionen --------------------------------------

    WertP methode(const KnotenP &k, Umgebung &u)
    {
        const std::string &name = k->text;
        const KnotenP &empfaenger = k->kinder[0];

        // Methoden, die den Empfaenger veraendern, brauchen seine Zelle.
        if (name == "push" || name == "push_str" || name == "clear" ||
            name == "pop" || name == "swap" || name == "sort" || name == "insert") {
            ZelleP z;
            WertP *platz = 0;
            bool ueber_verweis = false;
            stelle(empfaenger, u, z, platz, ueber_verweis);
            WertP ziel = *platz;
            if (ziel->art == Wertart::Verweis && ziel->ziel) {
                if (!ziel->ziel_veraenderlich)
                    werfen(k->zeile, k->spalte,
                           "error[E0596]: das ist eine gemeinsame Ausleihe `&` -- "
                           "aendern darf nur, wer `&mut` bekommen hat");
                z = ziel->ziel;
                ziel = z->wert;
            } else if (!z->veraenderlich) {
                werfen(k->zeile, k->spalte,
                       "error[E0596]: `" + z->name + "` ist nicht veraenderlich -- "
                       "`" + name + "` verlangt `let mut`");
            }
            if (name == "push") {
                if (ziel->art != Wertart::Liste)
                    werfen(k->zeile, k->spalte, "error[E0599]: `push` gibt es an `Vec`");
                if (k->kinder.size() < 2)
                    werfen(k->zeile, k->spalte, "error: `push` braucht einen Wert");
                ziel->liste.push_back(wertVon(k->kinder[1], u, true));
                return einheit();
            }
            if (name == "pop") {
                if (ziel->liste.empty())
                    return einheit();
                WertP letzter = ziel->liste.back();
                ziel->liste.pop_back();
                return letzter;
            }
            if (name == "clear") {
                ziel->liste.clear();
                ziel->text.clear();
                return einheit();
            }
            if (name == "push_str") {
                WertP s = wertVon(k->kinder[1], u, false);
                if (s->art == Wertart::Verweis && s->ziel) s = s->ziel->wert;
                ziel->text += s->text;
                return einheit();
            }
            if (name == "swap") {
                WertP a = wertVon(k->kinder[1], u, false);
                WertP b = wertVon(k->kinder[2], u, false);
                if (a->art != Wertart::Ganz || b->art != Wertart::Ganz)
                    werfen(k->zeile, k->spalte, "error: `swap` nimmt zwei Indizes");
                size_t i = (size_t)a->ganz, j = (size_t)b->ganz;
                if (i >= ziel->liste.size() || j >= ziel->liste.size())
                    werfen(k->zeile, k->spalte, "panicked: index out of bounds");
                std::swap(ziel->liste[i], ziel->liste[j]);
                return einheit();
            }
        }

        WertP a = wertVon(empfaenger, u, false);
        WertP roh = a;
        if (a->art == Wertart::Verweis && a->ziel)
            a = a->ziel->wert;

        if (name == "clone")
            return tiefekopie(a);
        if (name == "len") {
            if (a->art == Wertart::Liste) return ganzwert((long long)a->liste.size());
            if (a->art == Wertart::Text) return ganzwert((long long)a->text.size());
            werfen(k->zeile, k->spalte, "error[E0599]: `len` gibt es an `Vec` und `String`");
        }
        if (name == "is_empty") {
            if (a->art == Wertart::Liste) return wahrheitswert(a->liste.empty());
            return wahrheitswert(a->text.empty());
        }
        if (name == "iter" || name == "into_iter" || name == "as_str" ||
            name == "to_vec" || name == "as_slice")
            return a;
        if (name == "sum") {
            if (a->art != Wertart::Liste)
                werfen(k->zeile, k->spalte, "error[E0599]: `sum` gibt es nach `iter()`");
            if (a->liste.empty())
                return kommawert(0.0);
            if (a->liste[0]->art == Wertart::Ganz) {
                long long s = 0;
                for (size_t i = 0; i < a->liste.size(); i++) s += a->liste[i]->ganz;
                return ganzwert(s);
            }
            double s = 0;
            for (size_t i = 0; i < a->liste.size(); i++) s += a->liste[i]->komma;
            return kommawert(s);
        }
        if (name == "to_string") return textwert(zeichenfolge(a, "", k));

        // Zahlenmethoden. In Rust heissen sie so und nicht sqrt(x) -- eine
        // Umstellung, die Umsteigern aus C zuerst auffaellt.
        if (a->art == Wertart::Komma || a->art == Wertart::Ganz) {
            double x = a->art == Wertart::Komma ? a->komma : (double)a->ganz;
            bool ist_komma = a->art == Wertart::Komma;
            WertP arg = k->kinder.size() > 1 ? wertVon(k->kinder[1], u, false) : WertP();
            double y = 0;
            if (arg) {
                if (arg->art == Wertart::Verweis && arg->ziel) arg = arg->ziel->wert;
                y = arg->art == Wertart::Komma ? arg->komma : (double)arg->ganz;
            }
            if (name == "sqrt") return kommawert(sqrt(x));
            if (name == "sin") return kommawert(sin(x));
            if (name == "cos") return kommawert(cos(x));
            if (name == "tan") return kommawert(tan(x));
            if (name == "exp") return kommawert(exp(x));
            if (name == "ln") return kommawert(log(x));
            if (name == "log10") return kommawert(log10(x));
            if (name == "floor") return kommawert(floor(x));
            if (name == "ceil") return kommawert(ceil(x));
            if (name == "round") return kommawert(x < 0 ? -floor(-x + 0.5) : floor(x + 0.5));
            if (name == "abs") return ist_komma ? kommawert(fabs(x)) : ganzwert(a->ganz < 0 ? -a->ganz : a->ganz);
            if (name == "powi" || name == "pow") {
                if (ist_komma) return kommawert(pow(x, y));
                long long e = (long long)y, r = 1;
                for (long long i = 0; i < e; i++) r *= a->ganz;
                return ganzwert(r);
            }
            if (name == "powf") return kommawert(pow(x, y));
            if (name == "max") return ist_komma ? kommawert(x > y ? x : y) : ganzwert(a->ganz > (long long)y ? a->ganz : (long long)y);
            if (name == "min") return ist_komma ? kommawert(x < y ? x : y) : ganzwert(a->ganz < (long long)y ? a->ganz : (long long)y);
            if (name == "is_nan") return wahrheitswert(x != x);
        }
        (void)roh;
        werfen(k->zeile, k->spalte,
               "error[E0599]: die Methode `" + name + "` kennt dieser Deuter nicht");
        return einheit();
    }

    WertP aufruf(const KnotenP &k, Umgebung &u)
    {
        const std::string &name = k->text;

        if (name == "String::from" || name == "String::new") {
            if (k->kinder.empty())
                return textwert("");
            WertP a = wertVon(k->kinder[0], u, false);
            return textwert(a->text);
        }
        if (name == "Vec::new" || name == "Vec::with_capacity") {
            WertP v(new Wert);
            v->art = Wertart::Liste;
            return v;
        }
        // std::mem::swap(&mut a, &mut b) -- genau der Handgriff, mit dem
        // ein Loeser zwei Gitter tauscht, statt eines ueber das andere zu
        // kopieren.
        if (name == "std::mem::swap" || name == "mem::swap" || name == "swap") {
            if (k->kinder.size() != 2)
                werfen(k->zeile, k->spalte, "error: `swap` nimmt zwei `&mut`");
            WertP a = wertVon(k->kinder[0], u, false);
            WertP b = wertVon(k->kinder[1], u, false);
            if (a->art != Wertart::Verweis || b->art != Wertart::Verweis ||
                !a->ziel_veraenderlich || !b->ziel_veraenderlich)
                werfen(k->zeile, k->spalte,
                       "error[E0308]: `swap` verlangt zwei veraenderliche Ausleihen: "
                       "`std::mem::swap(&mut a, &mut b)`");
            std::swap(a->ziel->wert, b->ziel->wert);
            return einheit();
        }

        std::map<std::string, Funktion>::iterator it = funktionen.find(name);
        if (it == funktionen.end())
            werfen(k->zeile, k->spalte,
                   "error[E0425]: die Funktion `" + name + "` gibt es nicht");
        Funktion &f = it->second;
        if (f.parameter.size() != k->kinder.size()) {
            char puffer[200];
            snprintf(puffer, sizeof(puffer),
                     "error[E0061]: `%s` nimmt %d Argumente, hier stehen %d",
                     name.c_str(), (int)f.parameter.size(), (int)k->kinder.size());
            werfen(k->zeile, k->spalte, puffer);
        }

        // Argumente werden in der **aufrufenden** Umgebung ausgewertet.
        // Ein Wert, der nicht kopierbar ist, wandert dabei hinueber -- das
        // ist die Besitzuebergabe, die den Aufrufer leer zuruecklaesst.
        std::vector<WertP> werte;
        for (size_t i = 0; i < k->kinder.size(); i++) {
            bool verschiebend = !f.parameter[i].verweis;
            werte.push_back(wertVon(k->kinder[i], u, verschiebend));
        }

        Umgebung innen(0);          // keine Sicht auf die Umgebung des Aufrufers
        for (size_t i = 0; i < werte.size(); i++) {
            WertP w = werte[i];
            if (!f.parameter[i].verweis && !istKopierbar(w))
                w = w;              // der Wert wandert mit, die Zelle bleibt leer
            innen.anlegen(f.parameter[i].name, w, f.parameter[i].veraenderlich);
        }
        try {
            WertP letzter = block(f.rumpf, innen);
            return letzter;
        } catch (Rueckkehr &r) {
            return r.wert;
        }
    }
};

// ---------------------------------------------------------------------
// Hauptprogramm
// ---------------------------------------------------------------------

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Aufruf: rrun datei.rs\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "rrun: %s laesst sich nicht oeffnen\n", argv[1]);
        return 2;
    }
    std::string quelle;
    char puffer[4096];
    size_t n;
    while ((n = fread(puffer, 1, sizeof(puffer), f)) > 0)
        quelle.append(puffer, n);
    fclose(f);

    try {
        Zerleger z(quelle);
        std::vector<Wortmarke> marken = z.alles();
        Deuter d;
        Leser l(marken);
        l.programm(d.funktionen, d.verbunde);
        d.lauf();
    } catch (Fehler &e) {
        fflush(stdout);
        // Dieselbe Form, die auch picoc schreibt -- die App liest Zeile und
        // Spalte daraus und hebt die Stelle im Editor hervor.
        fprintf(stderr, "rrun:%d:%d %s\n", e.zeile, e.spalte, e.text.c_str());
        return 1;
    } catch (Rueckkehr &) {
        // `return` ganz oben in main: das Programm ist einfach fertig.
    } catch (Abbruch &) {
        fflush(stdout);
        fprintf(stderr, "rrun:0:0 error[E0268]: `break` steht ausserhalb einer Schleife\n");
        return 1;
    } catch (Naechster &) {
        fflush(stdout);
        fprintf(stderr, "rrun:0:0 error[E0268]: `continue` steht ausserhalb einer Schleife\n");
        return 1;
    }
    fflush(stdout);
    return 0;
}
