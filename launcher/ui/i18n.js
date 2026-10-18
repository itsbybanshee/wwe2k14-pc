// Launcher translations. The page is written in English; every visible text (and title / aria-label) is looked up
// here by its English text and replaced while the launcher's language is not English. "{}" in a key matches any
// value (a number, a file name) and is put back in the same place in the translation. Messages that are not found
// whole are translated sentence by sentence. The launcher follows the game's Language setting.

const I18N_LANGUAGES = { english: 'en', french: 'fr', german: 'de', italian: 'it', spanish: 'es' };
const I18N_LOCALES = { en: 'en-US', fr: 'fr-FR', de: 'de-DE', it: 'it-IT', es: 'es-ES' };

// [English, French, German, Italian, Spanish]
const I18N_ROWS = [
    // Header, panels, footer
    ['PC recompilation by Banshee', 'Recompilation PC par Banshee', 'PC-Rekompilierung von Banshee', 'Ricompilazione PC di Banshee', 'Recompilación para PC por Banshee'],
    ['Settings', 'Paramètres', 'Einstellungen', 'Impostazioni', 'Ajustes'],
    ['Game files', 'Fichiers du jeu', 'Spieldateien', 'File di gioco', 'Archivos del juego'],
    ['No folder chosen', 'Aucun dossier choisi', 'Kein Ordner ausgewählt', 'Nessuna cartella scelta', 'Ninguna carpeta elegida'],
    ['Choose folder', 'Choisir un dossier', 'Ordner wählen', 'Scegli cartella', 'Elegir carpeta'],
    ['Choose ISO', "Choisir l'ISO", 'ISO wählen', 'Scegli ISO', 'Elegir ISO'],
    ['Total Conversion', 'Conversion totale', 'Total Conversion', 'Conversione totale', 'Conversión total'],
    ['Restore Vanilla', "Restaurer l'original", 'Original wiederherstellen', 'Ripristina originale', 'Restaurar original'],
    ['Remove Update', 'Retirer la mise à jour', 'Update entfernen', 'Rimuovi aggiornamento', 'Quitar actualización'],
    ['Install update', 'Installer la mise à jour', 'Update installieren', 'Installa aggiornamento', 'Instalar actualización'],
    ['Vanilla', 'Original', 'Original', 'Originale', 'Original'],
    ['Updated', 'Mise à jour', 'Aktualisiert', 'Aggiornato', 'Actualizado'],
    ['Resolution', 'Résolution', 'Auflösung', 'Risoluzione', 'Resolución'],
    ['Graphics', 'Graphismes', 'Grafik', 'Grafica', 'Gráficos'],
    ['Syncs to your monitor to reduce tearing.', "Se synchronise avec l'écran pour éviter les déchirures d'image.", 'Synchronisiert mit dem Monitor, um Bildrisse zu vermeiden.', 'Si sincronizza con il monitor per evitare il tearing.', 'Se sincroniza con el monitor para evitar el tearing.'],
    ['60 FPS Cinematics', 'Cinématiques à 60 FPS', '60-FPS-Sequenzen', 'Filmati a 60 FPS', 'Cinemáticas a 60 FPS'],
    ['Renders entrances and victory motions at 60 FPS.', 'Entrées et célébrations de victoire à 60 FPS.', 'Einmärsche und Siegerposen mit 60 FPS.', 'Ingressi ed esultanze di vittoria a 60 FPS.', 'Entradas y celebraciones de victoria a 60 FPS.'],
    ['Shadows', 'Ombres', 'Schatten', 'Ombre', 'Sombras'],
    ['Dynamic character and ring shadows.', 'Ombres dynamiques des catcheurs et du ring.', 'Dynamische Schatten von Wrestlern und Ring.', 'Ombre dinamiche di lottatori e ring.', 'Sombras dinámicas de luchadores y ring.'],
    ['FPS Counter', 'Compteur de FPS', 'FPS-Anzeige', 'Contatore FPS', 'Contador de FPS'],
    ['Displays in-game framerate overlay.', 'Affiche les images par seconde en jeu.', 'Zeigt die Bildrate im Spiel an.', 'Mostra i fotogrammi al secondo in gioco.', 'Muestra los fotogramas por segundo en el juego.'],
    ['Window Mode', "Mode d'affichage", 'Fenstermodus', 'Modalità finestra', 'Modo de ventana'],
    ['Windowed, borderless, or fullscreen.', 'Fenêtré, sans bordure ou plein écran.', 'Fenster, randlos oder Vollbild.', 'In finestra, senza bordi o a schermo intero.', 'Ventana, sin bordes o pantalla completa.'],
    ['Off', 'Non', 'Aus', 'No', 'No'],
    ['Anisotropic Filtering', 'Filtrage anisotrope', 'Anisotrope Filterung', 'Filtro anisotropico', 'Filtrado anisotrópico'],
    ['Sharpens textures viewed at angles.', 'Textures plus nettes vues de biais.', 'Schärfere Texturen aus schrägen Winkeln.', 'Texture più nitide viste di sbieco.', 'Texturas más nítidas vistas en ángulo.'],
    ['Anti-Aliasing', 'Anticrénelage', 'Kantenglättung', 'Anti-aliasing', 'Antialiasing'],
    ['Smooths jagged edges.', 'Adoucit les bords crénelés.', 'Glättet gezackte Kanten.', 'Ammorbidisce i bordi seghettati.', 'Suaviza los bordes dentados.'],
    ['Upscaling', 'Scaling', 'Hochskalierung', 'Upscaling', 'Escalado'],
    ['Language', 'Langue', 'Sprache', 'Lingua', 'Idioma'],
    ['Achievements', 'Succès', 'Erfolge', 'Obiettivi', 'Logros'],
    ['Music', 'Musique', 'Musik', 'Musica', 'Música'],
    ['Logos', 'Logos', 'Logos', 'Loghi', 'Logotipos'],
    ['View all', 'Tout voir', 'Alle anzeigen', 'Vedi tutti', 'Ver todos'],
    ['Add packages', 'Ajouter des paquets', 'Pakete hinzufügen', 'Aggiungi pacchetti', 'Añadir paquetes'],
    ['Add a folder', 'Ajouter un dossier', 'Ordner hinzufügen', 'Aggiungi cartella', 'Añadir carpeta'],
    ['Open', 'Ouvrir', 'Öffnen', 'Apri', 'Abrir'],
    ['Xbox 360 DLC packages of your own copy (LIVE, PIRS or CON files).', 'Paquets DLC Xbox 360 de votre propre copie (fichiers LIVE, PIRS ou CON).', 'Xbox-360-DLC-Pakete deiner eigenen Kopie (LIVE-, PIRS- oder CON-Dateien).', 'Pacchetti DLC Xbox 360 della tua copia (file LIVE, PIRS o CON).', 'Paquetes DLC de Xbox 360 de tu propia copia (archivos LIVE, PIRS o CON).'],
    ['Custom music', 'Musique perso', 'Eigene Musik', 'Musica personalizzata', 'Música personalizada'],
    ['Add songs', 'Ajouter des morceaux', 'Songs hinzufügen', 'Aggiungi brani', 'Añadir canciones'],
    ['Formats: MP3, WAV, FLAC, OGG. Select "User playlist" in-game.', 'Formats : MP3, WAV, FLAC, OGG. Choisissez la playlist personnalisée dans le jeu.', 'Formate: MP3, WAV, FLAC, OGG. Wähle im Spiel die eigene Playlist.', 'Formati: MP3, WAV, FLAC, OGG. Scegli la playlist personalizzata nel gioco.', 'Formatos: MP3, WAV, FLAC, OGG. Elige la lista de reproducción personalizada en el juego.'],
    ['Custom logos', 'Logos perso', 'Eigene Logos', 'Loghi personalizzati', 'Logotipos personalizados'],
    ['Add logos', 'Ajouter des logos', 'Logos hinzufügen', 'Aggiungi loghi', 'Añadir logotipos'],
    ['Import PS3 save', 'Importer une sauvegarde PS3', 'PS3-Spielstand importieren', 'Importa salvataggio PS3', 'Importar partida de PS3'],
    ['Library', 'Bibliothèque', 'Bibliothek', 'Libreria', 'Biblioteca'],
    ['PNG, JPG, BMP or GIF, fitted to 256x256 with transparency. Up to 20 logos.', "PNG, JPG, BMP ou GIF, ajustés en 256x256 avec transparence. Jusqu'à 20 logos.", 'PNG, JPG, BMP oder GIF, auf 256x256 angepasst, mit Transparenz. Bis zu 20 Logos.', 'PNG, JPG, BMP o GIF, adattati a 256x256 con trasparenza. Fino a 20 loghi.', 'PNG, JPG, BMP o GIF, ajustados a 256x256 con transparencia. Hasta 20 logotipos.'],
    ['Add package', 'Ajouter un paquet', 'Paket hinzufügen', 'Aggiungi pacchetto', 'Añadir paquete'],
    ['Add folder', 'Ajouter un dossier', 'Ordner hinzufügen', 'Aggiungi cartella', 'Añadir carpeta'],
    ['Non-destructive mods: packages (.zip) or loose folders with pac/ and stream/ assets.', 'Mods non destructifs : paquets (.zip) ou dossiers avec des fichiers pac/ et stream/.', 'Nicht-destruktive Mods: Pakete (.zip) oder Ordner mit pac/- und stream/-Dateien.', 'Mod non distruttive: pacchetti (.zip) o cartelle con file pac/ e stream/.', 'Mods no destructivos: paquetes (.zip) o carpetas con archivos pac/ y stream/.'],
    ['Saves & Data', 'Sauvegardes et données', 'Spielstände & Daten', 'Salvataggi e dati', 'Partidas y datos'],
    ['Screenshots (F12)', "Captures d'écran (F12)", 'Screenshots (F12)', 'Screenshot (F12)', 'Capturas (F12)'],
    ['Controls', 'Commandes', 'Steuerung', 'Comandi', 'Controles'],
    ['Play', 'Jouer', 'Spielen', 'Gioca', 'Jugar'],

    // Achievements
    ['complete', 'terminé', 'abgeschlossen', 'completato', 'completado'],
    ['unlocked', 'débloqués', 'freigeschaltet', 'sbloccati', 'desbloqueados'],
    ['All', 'Tous', 'Alle', 'Tutti', 'Todos'],
    ['Unlocked', 'Débloqué', 'Freigeschaltet', 'Sbloccato', 'Desbloqueado'],
    ['Locked', 'Verrouillé', 'Gesperrt', 'Bloccato', 'Bloqueado'],
    ['Unlocked {}', 'Débloqué le {}', 'Freigeschaltet am {}', 'Sbloccato il {}', 'Desbloqueado el {}'],
    ['Achievement {}', 'Succès {}', 'Erfolg {}', 'Obiettivo {}', 'Logro {}'],
    ['{} of {} unlocked', '{} sur {} débloqués', '{} von {} freigeschaltet', '{} di {} sbloccati', '{} de {} desbloqueados'],
    ['Start the game once to load the achievement list.', 'Lancez le jeu une fois pour charger la liste des succès.', 'Starte das Spiel einmal, um die Erfolgsliste zu laden.', "Avvia il gioco una volta per caricare l'elenco degli obiettivi.", 'Inicia el juego una vez para cargar la lista de logros.'],
    ['No achievements unlocked yet.', "Aucun succès débloqué pour l'instant.", 'Noch keine Erfolge freigeschaltet.', 'Nessun obiettivo sbloccato finora.', 'Aún no has desbloqueado ningún logro.'],
    ['Every achievement is unlocked!', 'Tous les succès sont débloqués !', 'Alle Erfolge freigeschaltet!', 'Tutti gli obiettivi sono sbloccati!', '¡Todos los logros desbloqueados!'],
    ['Open the achievements window', 'Ouvrir la fenêtre des succès', 'Erfolgsfenster öffnen', 'Apri la finestra degli obiettivi', 'Abrir la ventana de logros'],

    // Logo library window
    ['Logo library', 'Bibliothèque de logos', 'Logo-Bibliothek', 'Libreria loghi', 'Biblioteca de logotipos'],
    ['Close the game before changing logos.', 'Fermez le jeu avant de modifier les logos.', 'Schließe das Spiel, bevor du Logos änderst.', 'Chiudi il gioco prima di modificare i loghi.', 'Cierra el juego antes de cambiar los logotipos.'],
    ['Got it', 'Compris', 'Verstanden', 'Ho capito', 'Entendido'],

    // Controls window
    ['Controller', 'Manette', 'Controller', 'Controller', 'Mando'],
    ['Keyboard', 'Clavier', 'Tastatur', 'Tastiera', 'Teclado'],
    ['Default gameplay controls (Control Type A).', 'Commandes par défaut (type de commandes A).', 'Standardsteuerung (Steuerungstyp A).', 'Comandi predefiniti (tipo di comandi A).', 'Controles predeterminados (tipo de control A).'],
    ['+ LS to ropes: apron / enter or exit ring; hold: pick up', '+ LS vers les cordes : tablier / entrer ou sortir du ring ; maintenir : ramasser', '+ LS zu den Seilen: Ringschürze / Ring betreten oder verlassen; halten: aufheben', '+ LS verso le corde: bordo ring / entra o esci dal ring; tieni premuto: raccogli', '+ LS hacia las cuerdas: delantal / entrar o salir del ring; mantener: recoger'],
    ['Reverse / block attacks', 'Contrer / bloquer les attaques', 'Angriffe kontern / blocken', 'Contromossa / blocca gli attacchi', 'Contraatacar / bloquear ataques'],
    ['Hold: pick up objects; with RS: Limb Target System', 'Maintenir : ramasser des objets ; avec RS : ciblage des membres', 'Halten: Gegenstände aufheben; mit RS: Körperteil-Zielsystem', 'Tieni premuto: raccogli oggetti; con RS: bersaglio degli arti', 'Mantener: recoger objetos; con RS: ataque a extremidades'],
    ['Hold + LS: run', 'Maintenir + LS : courir', 'Halten + LS: rennen', 'Tieni premuto + LS: corri', 'Mantener + LS: correr'],
    ['Signature / Finisher', 'Prise signature / Prise de finition', 'Signature / Finisher', 'Mossa caratteristica / Mossa finale', 'Movimiento característico / Movimiento final'],
    ['Irish whip / Pin (grounded opponent)', 'Irish whip / Tombé (adversaire au sol)', 'Irish Whip / Pin (Gegner am Boden)', 'Irish whip / Schienamento (avversario a terra)', 'Irish whip / Cuenta (rival en el suelo)'],
    ['Strike; hold: strong strike', 'Frappe ; maintenir : frappe puissante', 'Schlag; halten: starker Schlag', 'Colpo; tieni premuto: colpo potente', 'Golpe; mantener: golpe fuerte'],
    ['Grapple', 'Prise', 'Griff', 'Presa', 'Agarre'],
    ['Not used', 'Non utilisé', 'Nicht belegt', 'Non usato', 'Sin uso'],
    ['Pause', 'Pause', 'Pause', 'Pausa', 'Pausa'],
    ['Help menu (menus)', "Menu d'aide (menus)", 'Hilfemenü (Menüs)', 'Menu di aiuto (menu)', 'Menú de ayuda (menús)'],
    ['Change grapple hold, reposition opponent', "Changer de prise, repositionner l'adversaire", 'Griff wechseln, Gegner umpositionieren', "Cambia presa, riposiziona l'avversario", 'Cambiar de agarre, reposicionar al rival'],
    ['D-pad', 'Croix directionnelle', 'Steuerkreuz', 'Croce direzionale', 'Cruceta'],
    ['Taunts', 'Provocations', 'Provokationen', 'Provocazioni', 'Burlas'],
    ['Move; click: target another opponent', 'Se déplacer ; clic : cibler un autre adversaire', 'Bewegen; Klick: anderen Gegner anvisieren', 'Muoviti; clic: punta un altro avversario', 'Moverse; clic: apuntar a otro rival'],
    ['The keyboard plays as the controller: each key presses the button shown.', 'Le clavier remplace la manette : chaque touche appuie sur le bouton indiqué.', 'Die Tastatur ersetzt den Controller: Jede Taste drückt die angezeigte Controllertaste.', 'La tastiera funziona come il controller: ogni tasto preme il pulsante indicato.', 'El teclado funciona como el mando: cada tecla pulsa el botón indicado.'],
    ['Movement', 'Déplacement', 'Bewegung', 'Movimento', 'Movimiento'],
    ['or', 'ou', 'oder', 'o', 'o'],
    ['Arrows', 'Flèches', 'Pfeiltasten', 'Frecce', 'Flechas'],
    ['Move', 'Se déplacer', 'Bewegen', 'Muoviti', 'Moverse'],
    ['Run', 'Courir', 'Rennen', 'Corri', 'Correr'],
    ['Shift', 'Maj', 'Umschalt', 'Maiusc', 'Mayús'],
    ['Apron / enter or exit ring, pick up (hold)', 'Tablier / entrer ou sortir du ring, ramasser (maintenir)', 'Ringschürze / Ring betreten oder verlassen, aufheben (halten)', 'Bordo ring / entra o esci dal ring, raccogli (tieni premuto)', 'Delantal / entrar o salir del ring, recoger (mantener)'],
    ['Target another opponent', 'Cibler un autre adversaire', 'Anderen Gegner anvisieren', 'Punta un altro avversario', 'Apuntar a otro rival'],
    ['(LS click)', '(clic LS)', '(LS-Klick)', '(clic LS)', '(clic LS)'],
    ['Attack', 'Attaque', 'Angriff', 'Attacco', 'Ataque'],
    ['Space', 'Espace', 'Leertaste', 'Spazio', 'Espacio'],
    ['Backspace', 'Retour arrière', 'Rücktaste', 'Backspace', 'Retroceso'],
    ['Irish whip / Pin', 'Irish whip / Tombé', 'Irish Whip / Pin', 'Irish whip / Schienamento', 'Irish whip / Cuenta'],
    ['Mouse', 'Souris', 'Maus', 'Mouse', 'Ratón'],
    ['Change grapple hold, reposition', 'Changer de prise, repositionner', 'Griff wechseln, umpositionieren', 'Cambia presa, riposiziona', 'Cambiar de agarre, reposicionar'],
    ['(RS up, down, left, right)', '(RS haut, bas, gauche, droite)', '(RS hoch, runter, links, rechts)', '(RS su, giù, sinistra, destra)', '(RS arriba, abajo, izquierda, derecha)'],
    ['Right stick click', 'Clic du stick droit', 'Rechten Stick drücken', 'Clic levetta destra', 'Clic del stick derecho'],
    ['(RS click)', '(clic RS)', '(RS-Klick)', '(clic RS)', '(clic RS)'],
    ['Pick up objects; with RS keys: limb target', 'Ramasser des objets ; avec les touches RS : cibler un membre', 'Gegenstände aufheben; mit RS-Tasten: Körperteil anvisieren', 'Raccogli oggetti; con i tasti RS: bersaglio degli arti', 'Recoger objetos; con las teclas RS: atacar extremidades'],
    ['Defense & extras', 'Défense et extras', 'Verteidigung & Extras', 'Difesa ed extra', 'Defensa y extras'],
    ['Ctrl', 'Ctrl', 'Strg', 'Ctrl', 'Ctrl'],
    ['Reverse / block', 'Contrer / bloquer', 'Kontern / blocken', 'Contromossa / blocca', 'Contraatacar / bloquear'],
    ['(D-pad up, down, left, right)', '(croix haut, bas, gauche, droite)', '(Steuerkreuz hoch, runter, links, rechts)', '(croce su, giù, sinistra, destra)', '(cruceta arriba, abajo, izquierda, derecha)'],
    ['Enter', 'Entrée', 'Eingabe', 'Invio', 'Intro'],
    ['(Start; in menus Enter also confirms, Esc goes back)', '(Start ; dans les menus, Entrée valide aussi et Échap revient en arrière)', '(Start; in Menüs bestätigt Eingabe auch, Esc geht zurück)', '(Start; nei menu Invio conferma ed Esc torna indietro)', '(Start; en los menús Intro también confirma y Esc vuelve atrás)'],
    ['Help menu', "Menu d'aide", 'Hilfemenü', 'Menu di aiuto', 'Menú de ayuda'],
    ['Mouse (menus)', 'Souris (menus)', 'Maus (Menüs)', 'Mouse (menu)', 'Ratón (menús)'],
    ['Point', 'Pointer', 'Zeigen', 'Punta', 'Apuntar'],
    ['Select the item, wrestler or tab under the cursor', "Sélectionne l'élément, le catcheur ou l'onglet sous le curseur", 'Wählt das Element, den Wrestler oder den Tab unter dem Cursor', "Seleziona l'elemento, il lottatore o la scheda sotto il cursore", 'Selecciona el elemento, el luchador o la pestaña bajo el cursor'],
    ['Click', 'Clic', 'Klick', 'Clic', 'Clic'],
    ['Choose it; click a button prompt to press that button', 'Le choisit ; cliquez sur une icône de bouton pour appuyer dessus', 'Wählt es aus; ein Klick auf ein Tastensymbol drückt diese Taste', "Lo sceglie; clicca su un'icona di pulsante per premerlo", 'Lo elige; haz clic en un icono de botón para pulsarlo'],
    ['Wheel', 'Molette', 'Mausrad', 'Rotellina', 'Rueda'],
    ['Scroll up / down', 'Défiler vers le haut / le bas', 'Nach oben / unten scrollen', 'Scorri su / giù', 'Desplazarse arriba / abajo'],
    ['Middle click', 'Clic central', 'Mittelklick', 'Clic centrale', 'Clic central'],

    // Titles and labels
    ['Banshee on social media', 'Banshee sur les réseaux sociaux', 'Banshee in sozialen Medien', 'Banshee sui social', 'Banshee en redes sociales'],
    ['Banshee on X', 'Banshee sur X', 'Banshee auf X', 'Banshee su X', 'Banshee en X'],
    ['Banshee on Patreon', 'Banshee sur Patreon', 'Banshee auf Patreon', 'Banshee su Patreon', 'Banshee en Patreon'],
    ['Run the game straight from your disc image (.iso)', 'Lancer le jeu directement depuis votre image disque (.iso)', 'Das Spiel direkt aus deinem Disc-Image (.iso) starten', 'Avvia il gioco direttamente dalla tua immagine disco (.iso)', 'Ejecutar el juego directamente desde tu imagen de disco (.iso)'],
    ['1-Click install of Total Conversion mods like Wrestling Legends', 'Installation en un clic des conversions totales comme Wrestling Legends', 'Total-Conversion-Mods wie Wrestling Legends mit einem Klick installieren', 'Installazione con un clic di conversioni totali come Wrestling Legends', 'Instalación en un clic de conversiones totales como Wrestling Legends'],
    ['Restore original vanilla game folder and saves', "Restaurer le dossier du jeu et les sauvegardes d'origine", 'Originalen Spielordner und Spielstände wiederherstellen', 'Ripristina la cartella di gioco e i salvataggi originali', 'Restaurar la carpeta del juego y las partidas originales'],
    ['Remove or manage title update', 'Retirer ou gérer la mise à jour', 'Titel-Update entfernen oder verwalten', "Rimuovi o gestisci l'aggiornamento", 'Quitar o gestionar la actualización'],
    ['Game version', 'Version du jeu', 'Spielversion', 'Versione del gioco', 'Versión del juego'],
    ['Windowed', 'Fenêtré', 'Fenster', 'In finestra', 'Ventana'],
    ['Borderless', 'Sans bordure', 'Randlos', 'Senza bordi', 'Sin bordes'],
    ['Fullscreen', 'Plein écran', 'Vollbild', 'Schermo intero', 'Pantalla completa'],
    ['DLC and achievements', 'DLC et succès', 'DLC und Erfolge', 'DLC e obiettivi', 'DLC y logros'],
    ['Panel', 'Panneau', 'Bereich', 'Pannello', 'Panel'],
    ['Close', 'Fermer', 'Schließen', 'Chiudi', 'Cerrar'],
    ['Show', 'Afficher', 'Anzeigen', 'Mostra', 'Mostrar'],
    ['Xbox 360 controller', 'Manette Xbox 360', 'Xbox-360-Controller', 'Controller Xbox 360', 'Mando de Xbox 360'],
    ['Tap + LS toward ropes: step on apron. Hold + LS toward ropes: enter / exit ring. Hold: pick up object.', 'Appui + LS vers les cordes : monter sur le tablier. Maintenir + LS vers les cordes : entrer / sortir du ring. Maintenir : ramasser un objet.', 'Tippen + LS zu den Seilen: auf die Ringschürze. Halten + LS zu den Seilen: Ring betreten / verlassen. Halten: Gegenstand aufheben.', 'Premi + LS verso le corde: sali sul bordo ring. Tieni premuto + LS verso le corde: entra / esci dal ring. Tieni premuto: raccogli un oggetto.', 'Pulsar + LS hacia las cuerdas: subir al delantal. Mantener + LS hacia las cuerdas: entrar / salir del ring. Mantener: recoger un objeto.'],

    // Resolution hints
    ['Native Xbox 360 resolution (1280×720). Lightest on the GPU.', 'Résolution native Xbox 360 (1280×720). La plus légère pour le GPU.', 'Native Xbox-360-Auflösung (1280×720). Am schonendsten für die GPU.', 'Risoluzione nativa Xbox 360 (1280×720). La più leggera per la GPU.', 'Resolución nativa de Xbox 360 (1280×720). La más ligera para la GPU.'],
    ['Rendered at 2560×1440, shown at 1920×1080: sharper than native 1080p.', 'Rendu en 2560×1440, affiché en 1920×1080 : plus net que le 1080p natif.', 'In 2560×1440 berechnet, in 1920×1080 angezeigt: schärfer als natives 1080p.', 'Renderizzato a 2560×1440, mostrato a 1920×1080: più nitido del 1080p nativo.', 'Renderizado a 2560×1440 y mostrado a 1920×1080: más nítido que el 1080p nativo.'],
    ['Rendered at 2560×1440, twice the console resolution.', 'Rendu en 2560×1440, deux fois la résolution de la console.', 'In 2560×1440 berechnet, doppelte Konsolenauflösung.', 'Renderizzato a 2560×1440, il doppio della risoluzione della console.', 'Renderizado a 2560×1440, el doble de la resolución de la consola.'],
    ['Rendered at 2560×1440 and scaled up to 3840×2160.', 'Rendu en 2560×1440 et agrandi en 3840×2160.', 'In 2560×1440 berechnet und auf 3840×2160 hochskaliert.', 'Renderizzato a 2560×1440 e scalato a 3840×2160.', 'Renderizado a 2560×1440 y escalado a 3840×2160.'],

    // Game files, title update
    ['Game files ready', 'Fichiers du jeu prêts', 'Spieldateien bereit', 'File di gioco pronti', 'Archivos del juego listos'],
    ['Disc image ready', 'Image disque prête', 'Disc-Image bereit', 'Immagine disco pronta', 'Imagen de disco lista'],
    ['Choose the folder of your extracted WWE 2K14 disc, or its .iso.', 'Choisissez le dossier de votre disque WWE 2K14 extrait, ou son .iso.', 'Wähle den Ordner deiner entpackten WWE-2K14-Disc oder ihre .iso.', 'Scegli la cartella del tuo disco WWE 2K14 estratto o il suo .iso.', 'Elige la carpeta de tu disco de WWE 2K14 extraído, o su .iso.'],
    ['This disc image no longer exists.', "Cette image disque n'existe plus.", 'Dieses Disc-Image existiert nicht mehr.', 'Questa immagine disco non esiste più.', 'Esta imagen de disco ya no existe.'],
    ['Not an Xbox 360 disc image.', "Ce n'est pas une image disque Xbox 360.", 'Kein Xbox-360-Disc-Image.', "Non è un'immagine disco Xbox 360.", 'No es una imagen de disco de Xbox 360.'],
    ['This folder no longer exists.', "Ce dossier n'existe plus.", 'Dieser Ordner existiert nicht mehr.', 'Questa cartella non esiste più.', 'Esta carpeta ya no existe.'],
    ['No default.xex in this folder.', 'Pas de default.xex dans ce dossier.', 'Keine default.xex in diesem Ordner.', 'Nessun default.xex in questa cartella.', 'No hay default.xex en esta carpeta.'],
    ['Building the updated game in the console window. This takes a while; the launcher updates when it is done.', 'Compilation du jeu mis à jour dans la fenêtre de console. Cela prend un moment ; le launcher se met à jour à la fin.', 'Das aktualisierte Spiel wird im Konsolenfenster erstellt. Das dauert etwas; der Launcher aktualisiert sich danach.', "Compilazione del gioco aggiornato nella finestra della console. Richiede un po' di tempo; il launcher si aggiorna al termine.", 'Compilando el juego actualizado en la ventana de la consola. Tarda un poco; el launcher se actualizará al terminar.'],
    ['Playing the original disc version. Have the title update file (tu00000002_00000000)? Install it here.', 'Version du disque d’origine. Vous avez le fichier de mise à jour (tu00000002_00000000) ? Installez-le ici.', 'Du spielst die Original-Disc-Version. Hast du die Update-Datei (tu00000002_00000000)? Installiere sie hier.', 'Stai giocando la versione originale del disco. Hai il file di aggiornamento (tu00000002_00000000)? Installalo qui.', 'Juegas a la versión original del disco. ¿Tienes el archivo de actualización (tu00000002_00000000)? Instálalo aquí.'],
    ['{} installed. Build it once to play the updated game.', '{} installée. Compilez-la une fois pour jouer au jeu mis à jour.', '{} installiert. Einmal erstellen, um das aktualisierte Spiel zu spielen.', '{} installato. Compilalo una volta per giocare alla versione aggiornata.', '{} instalada. Compílala una vez para jugar a la versión actualizada.'],
    ['Playing with {}.', 'Vous jouez avec {}.', 'Du spielst mit {}.', 'Stai giocando con {}.', 'Jugando con {}.'],
    ['{} ready. Pick "Updated" to play with it.', '{} prête. Choisissez « Mise à jour » pour jouer avec.', '{} bereit. Wähle „Aktualisiert“, um damit zu spielen.', '{} pronto. Scegli "Aggiornato" per giocarci.', '{} lista. Elige «Actualizado» para jugar con ella.'],
    ['Install a title update first', "Installez d'abord une mise à jour", 'Installiere zuerst ein Titel-Update', 'Installa prima un aggiornamento', 'Instala primero una actualización'],
    ['wwe2k14.exe was not found. Build the game first.', "wwe2k14.exe introuvable. Compilez d'abord le jeu.", 'wwe2k14.exe wurde nicht gefunden. Erstelle zuerst das Spiel.', 'wwe2k14.exe non trovato. Compila prima il gioco.', 'No se encontró wwe2k14.exe. Compila primero el juego.'],
    ['Close the game before installing the title update.', "Fermez le jeu avant d'installer la mise à jour.", 'Schließe das Spiel, bevor du das Titel-Update installierst.', "Chiudi il gioco prima di installare l'aggiornamento.", 'Cierra el juego antes de instalar la actualización.'],
    ['Title update installed.', 'Mise à jour installée.', 'Titel-Update installiert.', 'Aggiornamento installato.', 'Actualización instalada.'],
    ['Title update ready.', 'Mise à jour prête.', 'Titel-Update bereit.', 'Aggiornamento pronto.', 'Actualización lista.'],
    ['Close the game before removing the title update.', 'Fermez le jeu avant de retirer la mise à jour.', 'Schließe das Spiel, bevor du das Titel-Update entfernst.', "Chiudi il gioco prima di rimuovere l'aggiornamento.", 'Cierra el juego antes de quitar la actualización.'],
    ['Title update removed. Playing the original version.', "Mise à jour retirée. Retour à la version d'origine.", 'Titel-Update entfernt. Du spielst die Originalversion.', 'Aggiornamento rimosso. Ora giochi la versione originale.', 'Actualización quitada. Juegas a la versión original.'],
    ['This is DLC, not a title update: add it in the DLC tab.', "Ceci est un DLC, pas une mise à jour : ajoutez-le dans l'onglet DLC.", 'Das ist ein DLC, kein Titel-Update: Füge es im DLC-Tab hinzu.', 'Questo è un DLC, non un aggiornamento: aggiungilo nella scheda DLC.', 'Esto es un DLC, no una actualización: añádelo en la pestaña DLC.'],
    ['Not a title update (content type {}).', "Ce n'est pas une mise à jour (type de contenu {}).", 'Kein Titel-Update (Inhaltstyp {}).', 'Non è un aggiornamento (tipo di contenuto {}).', 'No es una actualización (tipo de contenido {}).'],
    ['This package has no default.xexp: not a WWE 2K14 title update.', "Ce paquet n'a pas de default.xexp : ce n'est pas une mise à jour de WWE 2K14.", 'Dieses Paket hat keine default.xexp: kein WWE-2K14-Titel-Update.', 'Questo pacchetto non ha default.xexp: non è un aggiornamento di WWE 2K14.', 'Este paquete no tiene default.xexp: no es una actualización de WWE 2K14.'],

    // Game start
    ['The game is already running.', 'Le jeu est déjà lancé.', 'Das Spiel läuft bereits.', 'Il gioco è già in esecuzione.', 'El juego ya se está ejecutando.'],
    ['The game could not start: {}', "Le jeu n'a pas pu démarrer : {}", 'Das Spiel konnte nicht starten: {}', 'Impossibile avviare il gioco: {}', 'No se pudo iniciar el juego: {}'],
    ['The game closed unexpectedly (code {}). The log is in the logs folder.', "Le jeu s'est fermé de façon inattendue (code {}). Le journal se trouve dans le dossier logs.", 'Das Spiel wurde unerwartet beendet (Code {}). Das Protokoll liegt im Ordner logs.', 'Il gioco si è chiuso in modo imprevisto (codice {}). Il log è nella cartella logs.', 'El juego se cerró inesperadamente (código {}). El registro está en la carpeta logs.'],
    ['Settings could not be saved: {}', "Impossible d'enregistrer les paramètres : {}", 'Einstellungen konnten nicht gespeichert werden: {}', 'Impossibile salvare le impostazioni: {}', 'No se pudieron guardar los ajustes: {}'],

    // DLC
    ['No DLC installed yet.', 'Aucun DLC installé.', 'Noch keine DLC installiert.', 'Nessun DLC installato.', 'Aún no hay DLC instalados.'],
    ['Remove "{}"? You can add the package again later.', 'Retirer « {} » ? Vous pourrez rajouter le paquet plus tard.', '„{}“ entfernen? Du kannst das Paket später wieder hinzufügen.', 'Rimuovere "{}"? Potrai aggiungere di nuovo il pacchetto in seguito.', '¿Quitar «{}»? Podrás volver a añadir el paquete más tarde.'],
    ['{} DLC packages installed.', '{} paquets DLC installés.', '{} DLC-Pakete installiert.', '{} pacchetti DLC installati.', '{} paquetes DLC instalados.'],
    ['{} installed.', '{} installé.', '{} installiert.', '{} installato.', '{} instalado.'],
    ['Already installed.', 'Déjà installé.', 'Bereits installiert.', 'Già installato.', 'Ya instalado.'],
    ['Installing {}', 'Installation de {}', '{} wird installiert', 'Installazione di {}', 'Instalando {}'],
    ['Installed.', 'Installé.', 'Installiert.', 'Installato.', 'Instalado.'],
    ['This is a title update, not DLC.', "Ceci est une mise à jour, pas un DLC.", 'Das ist ein Titel-Update, kein DLC.', 'Questo è un aggiornamento, non un DLC.', 'Esto es una actualización, no un DLC.'],
    ['This is a saved game, not DLC.', 'Ceci est une sauvegarde, pas un DLC.', 'Das ist ein Spielstand, kein DLC.', 'Questo è un salvataggio, non un DLC.', 'Esto es una partida guardada, no un DLC.'],
    ['This is a gamer profile, not DLC.', 'Ceci est un profil de joueur, pas un DLC.', 'Das ist ein Spielerprofil, kein DLC.', 'Questo è un profilo giocatore, non un DLC.', 'Esto es un perfil de jugador, no un DLC.'],
    ['Not a DLC package (content type {}).', "Ce n'est pas un paquet DLC (type de contenu {}).", 'Kein DLC-Paket (Inhaltstyp {}).', 'Non è un pacchetto DLC (tipo di contenuto {}).', 'No es un paquete DLC (tipo de contenido {}).'],
    ['This DLC is for another game ({}).', 'Ce DLC est pour un autre jeu ({}).', 'Dieser DLC gehört zu einem anderen Spiel ({}).', 'Questo DLC è per un altro gioco ({}).', 'Este DLC es de otro juego ({}).'],
    ['Invalid DLC name.', 'Nom de DLC non valide.', 'Ungültiger DLC-Name.', 'Nome DLC non valido.', 'Nombre de DLC no válido.'],
    ['No Xbox 360 packages were found in that folder.', 'Aucun paquet Xbox 360 trouvé dans ce dossier.', 'In diesem Ordner wurden keine Xbox-360-Pakete gefunden.', 'Nessun pacchetto Xbox 360 trovato in quella cartella.', 'No se encontraron paquetes de Xbox 360 en esa carpeta.'],
    ['DLC is already being installed.', "Des DLC sont déjà en cours d'installation.", 'DLC werden bereits installiert.', 'I DLC sono già in installazione.', 'Ya se están instalando DLC.'],
    ['Close the game before adding DLC.', "Fermez le jeu avant d'ajouter des DLC.", 'Schließe das Spiel, bevor du DLC hinzufügst.', 'Chiudi il gioco prima di aggiungere DLC.', 'Cierra el juego antes de añadir DLC.'],
    ['Close the game before removing DLC.', 'Fermez le jeu avant de retirer des DLC.', 'Schließe das Spiel, bevor du DLC entfernst.', 'Chiudi il gioco prima di rimuovere DLC.', 'Cierra el juego antes de quitar DLC.'],
    ['DLC removed.', 'DLC retiré.', 'DLC entfernt.', 'DLC rimosso.', 'DLC quitado.'],
    ['Could not remove it: {}', 'Impossible de le retirer : {}', 'Konnte nicht entfernt werden: {}', 'Impossibile rimuoverlo: {}', 'No se pudo quitar: {}'],
    ['File too small to be an Xbox 360 package.', 'Fichier trop petit pour être un paquet Xbox 360.', 'Datei zu klein für ein Xbox-360-Paket.', 'File troppo piccolo per essere un pacchetto Xbox 360.', 'Archivo demasiado pequeño para ser un paquete de Xbox 360.'],
    ['Not an Xbox 360 package (no CON/LIVE/PIRS header).', "Ce n'est pas un paquet Xbox 360 (pas d'en-tête CON/LIVE/PIRS).", 'Kein Xbox-360-Paket (kein CON/LIVE/PIRS-Header).', 'Non è un pacchetto Xbox 360 (nessuna intestazione CON/LIVE/PIRS).', 'No es un paquete de Xbox 360 (sin cabecera CON/LIVE/PIRS).'],

    // Mods
    ['No mods installed yet. Add a package (.zip) or folder to get started.', 'Aucun mod installé. Ajoutez un paquet (.zip) ou un dossier pour commencer.', 'Noch keine Mods installiert. Füge ein Paket (.zip) oder einen Ordner hinzu.', 'Nessuna mod installata. Aggiungi un pacchetto (.zip) o una cartella per iniziare.', 'Aún no hay mods instalados. Añade un paquete (.zip) o una carpeta para empezar.'],
    ['Enabled (click to disable)', 'Activé (cliquez pour désactiver)', 'Aktiviert (klicken zum Deaktivieren)', 'Attiva (clicca per disattivare)', 'Activado (haz clic para desactivar)'],
    ['Disabled (click to enable)', 'Désactivé (cliquez pour activer)', 'Deaktiviert (klicken zum Aktivieren)', 'Disattivata (clicca per attivare)', 'Desactivado (haz clic para activar)'],
    ['Remove mod "{}"?', 'Retirer le mod « {} » ?', 'Mod „{}“ entfernen?', 'Rimuovere la mod "{}"?', '¿Quitar el mod «{}»?'],
    ['Remove {}', 'Retirer {}', '{} entfernen', 'Rimuovi {}', 'Quitar {}'],
    ['Close the game before installing mods.', "Fermez le jeu avant d'installer des mods.", 'Schließe das Spiel, bevor du Mods installierst.', 'Chiudi il gioco prima di installare mod.', 'Cierra el juego antes de instalar mods.'],
    ["Mod '{}' installed.", "Mod « {} » installé.", 'Mod „{}“ installiert.', "Mod '{}' installata.", "Mod «{}» instalado."],
    ['{} mods installed.', '{} mods installés.', '{} Mods installiert.', '{} mod installate.', '{} mods instalados.'],
    ["Mod folder '{}' imported.", "Dossier de mod « {} » importé.", 'Mod-Ordner „{}“ importiert.', "Cartella mod '{}' importata.", "Carpeta de mod «{}» importada."],
    ['Close the game before removing mods.', 'Fermez le jeu avant de retirer des mods.', 'Schließe das Spiel, bevor du Mods entfernst.', 'Chiudi il gioco prima di rimuovere mod.', 'Cierra el juego antes de quitar mods.'],
    ['Mod removed.', 'Mod retiré.', 'Mod entfernt.', 'Mod rimossa.', 'Mod quitado.'],
    ['Could not remove mod.', 'Impossible de retirer le mod.', 'Mod konnte nicht entfernt werden.', 'Impossibile rimuovere la mod.', 'No se pudo quitar el mod.'],
    ['Mod installed successfully.', 'Mod installé.', 'Mod installiert.', 'Mod installata.', 'Mod instalado.'],
    ['Installation failed: {}', "Échec de l'installation : {}", 'Installation fehlgeschlagen: {}', 'Installazione non riuscita: {}', 'Error en la instalación: {}'],
    ['Mod folder imported.', 'Dossier de mod importé.', 'Mod-Ordner importiert.', 'Cartella mod importata.', 'Carpeta de mod importada.'],
    ['Import failed: {}', "Échec de l'importation : {}", 'Import fehlgeschlagen: {}', 'Importazione non riuscita: {}', 'Error en la importación: {}'],
    ['File added to mods.', 'Fichier ajouté aux mods.', 'Datei zu den Mods hinzugefügt.', 'File aggiunto alle mod.', 'Archivo añadido a los mods.'],
    ['Source folder does not exist.', "Le dossier source n'existe pas.", 'Der Quellordner existiert nicht.', 'La cartella di origine non esiste.', 'La carpeta de origen no existe.'],
    ['Video / Titantron', 'Vidéo / Titantron', 'Video / Titantron', 'Video / Titantron', 'Vídeo / Titantron'],
    ['Standalone Asset', 'Fichier isolé', 'Einzelne Datei', 'File singolo', 'Archivo suelto'],
    ['UI & Menus', 'Interface et menus', 'Oberfläche & Menüs', 'Interfaccia e menu', 'Interfaz y menús'],

    // Total conversion
    ['Close the game before installing a total conversion.', "Fermez le jeu avant d'installer une conversion totale.", 'Schließe das Spiel, bevor du eine Total Conversion installierst.', 'Chiudi il gioco prima di installare una conversione totale.', 'Cierra el juego antes de instalar una conversión total.'],
    ['Total conversion installation failed: {}', "Échec de l'installation de la conversion totale : {}", 'Installation der Total Conversion fehlgeschlagen: {}', 'Installazione della conversione totale non riuscita: {}', 'Error al instalar la conversión total: {}'],
    ['Close the game before restoring vanilla.', "Fermez le jeu avant de restaurer l'original.", 'Schließe das Spiel, bevor du das Original wiederherstellst.', "Chiudi il gioco prima di ripristinare l'originale.", 'Cierra el juego antes de restaurar el original.'],
    ['Selected folder does not exist.', "Le dossier choisi n'existe pas.", 'Der gewählte Ordner existiert nicht.', 'La cartella scelta non esiste.', 'La carpeta elegida no existe.'],
    ['Scanning mod folder...', 'Analyse du dossier du mod…', 'Mod-Ordner wird durchsucht …', 'Analisi della cartella mod…', 'Analizando la carpeta del mod…'],
    ['No valid WWE 2K14 game files, DLC packages, or save data found in the selected folder.', 'Aucun fichier de jeu WWE 2K14, paquet DLC ou sauvegarde valide dans le dossier choisi.', 'Im gewählten Ordner wurden keine gültigen WWE-2K14-Spieldateien, DLC-Pakete oder Spielstände gefunden.', 'Nessun file di gioco WWE 2K14, pacchetto DLC o salvataggio valido nella cartella scelta.', 'No se encontraron archivos de juego de WWE 2K14, paquetes DLC ni partidas válidos en la carpeta elegida.'],
    ['Installing DLC ({}/{}): {}', 'Installation des DLC ({}/{}) : {}', 'DLC werden installiert ({}/{}): {}', 'Installazione DLC ({}/{}): {}', 'Instalando DLC ({}/{}): {}'],
    ['Extracting save data ({}/{}): {}', 'Extraction des sauvegardes ({}/{}) : {}', 'Spielstände werden entpackt ({}/{}): {}', 'Estrazione dei salvataggi ({}/{}): {}', 'Extrayendo partidas ({}/{}): {}'],
    ['Failed to install save {}: {}', "Échec de l'installation de la sauvegarde {} : {}", 'Spielstand {} konnte nicht installiert werden: {}', 'Impossibile installare il salvataggio {}: {}', 'No se pudo instalar la partida {}: {}'],
    ['Finishing up...', 'Finalisation…', 'Wird abgeschlossen …', 'Completamento…', 'Terminando…'],
    ['Total Conversion installed!', 'Conversion totale installée !', 'Total Conversion installiert!', 'Conversione totale installata!', '¡Conversión total instalada!'],
    ['Could not restore vanilla saves: {}', "Impossible de restaurer les sauvegardes d'origine : {}", 'Original-Spielstände konnten nicht wiederhergestellt werden: {}', 'Impossibile ripristinare i salvataggi originali: {}', 'No se pudieron restaurar las partidas originales: {}'],
    ['Restored {} vanilla save items and switched to Vanilla version.', "{} éléments de sauvegarde d'origine restaurés, retour à la version originale.", '{} Original-Spielstände wiederhergestellt und zur Originalversion gewechselt.', '{} salvataggi originali ripristinati, passato alla versione originale.', '{} partidas originales restauradas y cambio a la versión original.'],
    ['Switched back to Vanilla version.', 'Retour à la version originale.', 'Zurück zur Originalversion gewechselt.', 'Tornato alla versione originale.', 'Vuelta a la versión original.'],

    // Music
    ['No songs yet. Add some to use them in game.', "Aucun morceau pour l'instant. Ajoutez-en pour les écouter en jeu.", 'Noch keine Songs. Füge welche hinzu, um sie im Spiel zu nutzen.', 'Nessun brano. Aggiungine per usarli nel gioco.', 'Aún no hay canciones. Añade algunas para usarlas en el juego.'],
    ['1 song added.', '1 morceau ajouté.', '1 Song hinzugefügt.', '1 brano aggiunto.', '1 canción añadida.'],
    ['{} songs added.', '{} morceaux ajoutés.', '{} Songs hinzugefügt.', '{} brani aggiunti.', '{} canciones añadidas.'],
    ['Song removed.', 'Morceau retiré.', 'Song entfernt.', 'Brano rimosso.', 'Canción quitada.'],

    // Logos
    ['{} in library · {} / 20 in game', '{} dans la bibliothèque · {} / 20 dans le jeu', '{} in der Bibliothek · {} / 20 im Spiel', '{} in libreria · {} / 20 nel gioco', '{} en la biblioteca · {} / 20 en el juego'],
    ['Click a logo to put it in the game or take it out (check mark = in the game).', "Cliquez sur un logo pour l'ajouter au jeu ou l'en retirer (coche = dans le jeu).", 'Klicke auf ein Logo, um es ins Spiel zu übernehmen oder herauszunehmen (Haken = im Spiel).', 'Clicca su un logo per metterlo nel gioco o toglierlo (spunta = nel gioco).', 'Haz clic en un logotipo para ponerlo en el juego o quitarlo (marca = en el juego).'],
    ['PNG, JPG, BMP or GIF, fitted to 256x256 with transparency. Up to 20 in the game, as many as you like in the Library.', "PNG, JPG, BMP ou GIF, ajustés en 256x256 avec transparence. Jusqu'à 20 dans le jeu, autant que vous voulez dans la bibliothèque.", 'PNG, JPG, BMP oder GIF, auf 256x256 angepasst, mit Transparenz. Bis zu 20 im Spiel, beliebig viele in der Bibliothek.', 'PNG, JPG, BMP o GIF, adattati a 256x256 con trasparenza. Fino a 20 nel gioco, quanti ne vuoi in libreria.', 'PNG, JPG, BMP o GIF, ajustados a 256x256 con transparencia. Hasta 20 en el juego, todos los que quieras en la biblioteca.'],
    ['Your library is empty. Add logos or import a PS3 save.', 'Votre bibliothèque est vide. Ajoutez des logos ou importez une sauvegarde PS3.', 'Deine Bibliothek ist leer. Füge Logos hinzu oder importiere einen PS3-Spielstand.', 'La tua libreria è vuota. Aggiungi loghi o importa un salvataggio PS3.', 'Tu biblioteca está vacía. Añade logotipos o importa una partida de PS3.'],
    ['In the game: click to take it out', 'Dans le jeu : cliquez pour le retirer', 'Im Spiel: klicken zum Herausnehmen', 'Nel gioco: clicca per toglierlo', 'En el juego: haz clic para quitarlo'],
    ['Click to put it in the game', "Cliquez pour l'ajouter au jeu", 'Klicken, um es ins Spiel zu übernehmen', 'Clicca per metterlo nel gioco', 'Haz clic para ponerlo en el juego'],
    ['Library logo {}', 'Logo de la bibliothèque {}', 'Bibliothekslogo {}', 'Logo della libreria {}', 'Logotipo de la biblioteca {}'],
    ['Delete from the library', 'Supprimer de la bibliothèque', 'Aus der Bibliothek löschen', 'Elimina dalla libreria', 'Eliminar de la biblioteca'],
    ['No logo save yet. In the game, save a logo once in the Paint Tool.', "Pas encore de sauvegarde de logos. Dans le jeu, enregistrez un logo une fois dans le Paint Tool.", 'Noch kein Logo-Spielstand. Speichere im Spiel einmal ein Logo im Paint Tool.', 'Ancora nessun salvataggio dei loghi. Nel gioco, salva una volta un logo nel Paint Tool.', 'Aún no hay partida de logotipos. En el juego, guarda un logotipo una vez en el Paint Tool.'],
    ['No custom logo save yet: in the game, open Creation Suite > Paint Tool and save a logo once.', 'Pas encore de sauvegarde de logos : dans le jeu, ouvrez Creation Suite > Paint Tool et enregistrez un logo une fois.', 'Noch kein Logo-Spielstand: Öffne im Spiel Creation Suite > Paint Tool und speichere einmal ein Logo.', 'Ancora nessun salvataggio dei loghi: nel gioco apri Creation Suite > Paint Tool e salva un logo una volta.', 'Aún no hay partida de logotipos: en el juego, abre Creation Suite > Paint Tool y guarda un logotipo una vez.'],
    ['Default logo', 'Logo par défaut', 'Standardlogo', 'Logo predefinito', 'Logotipo predeterminado'],
    ['Default', 'Par défaut', 'Standard', 'Predefinito', 'Predeterminado'],
    ['Custom logo {}', 'Logo perso {}', 'Eigenes Logo {}', 'Logo personalizzato {}', 'Logotipo personalizado {}'],
    ['Take custom logo {} out of the game (it stays in the Library)', 'Retirer le logo perso {} du jeu (il reste dans la bibliothèque)', 'Eigenes Logo {} aus dem Spiel nehmen (bleibt in der Bibliothek)', 'Togli il logo personalizzato {} dal gioco (resta in libreria)', 'Quitar el logotipo personalizado {} del juego (se queda en la biblioteca)'],
    ["The game's default logo can't be removed.", 'Le logo par défaut du jeu ne peut pas être retiré.', 'Das Standardlogo des Spiels kann nicht entfernt werden.', 'Il logo predefinito del gioco non può essere rimosso.', 'El logotipo predeterminado del juego no se puede quitar.'],
    ['All 20 logo slots in the game are used. Take one out first.', "Les 20 emplacements de logos du jeu sont pris. Retirez-en un d'abord.", 'Alle 20 Logo-Plätze im Spiel sind belegt. Nimm zuerst eines heraus.', 'Tutti i 20 spazi per i loghi del gioco sono occupati. Togline uno prima.', 'Los 20 huecos de logotipos del juego están ocupados. Quita uno primero.'],
    ['Logo deleted from the library.', 'Logo supprimé de la bibliothèque.', 'Logo aus der Bibliothek gelöscht.', 'Logo eliminato dalla libreria.', 'Logotipo eliminado de la biblioteca.'],
    ['Logo taken out of the game. It stays in your Library.', 'Logo retiré du jeu. Il reste dans votre bibliothèque.', 'Logo aus dem Spiel genommen. Es bleibt in deiner Bibliothek.', 'Logo tolto dal gioco. Resta nella tua libreria.', 'Logotipo quitado del juego. Se queda en tu biblioteca.'],
    ['That logo no longer exists.', "Ce logo n'existe plus.", 'Dieses Logo existiert nicht mehr.', 'Questo logo non esiste più.', 'Ese logotipo ya no existe.'],
    ['That logo is no longer in the library.', "Ce logo n'est plus dans la bibliothèque.", 'Dieses Logo ist nicht mehr in der Bibliothek.', 'Questo logo non è più nella libreria.', 'Ese logotipo ya no está en la biblioteca.'],
    ['{} could not be read as an image.', "{} n'a pas pu être lu comme une image.", '{} konnte nicht als Bild gelesen werden.', 'Impossibile leggere {} come immagine.', 'No se pudo leer {} como imagen.'],
    ['No LOGOS.DAT in there: choose a WWE 2K14 PS3 save folder, its LOGOS.DAT, or a .zip / .rar of the save.', 'Aucun LOGOS.DAT ici : choisissez un dossier de sauvegarde PS3 de WWE 2K14, son LOGOS.DAT, ou un .zip / .rar de la sauvegarde.', 'Keine LOGOS.DAT gefunden: Wähle einen WWE-2K14-PS3-Spielstandordner, seine LOGOS.DAT oder eine .zip / .rar des Spielstands.', 'Nessun LOGOS.DAT qui: scegli una cartella di salvataggio PS3 di WWE 2K14, il suo LOGOS.DAT o un .zip / .rar del salvataggio.', 'No hay ningún LOGOS.DAT: elige una carpeta de partida de PS3 de WWE 2K14, su LOGOS.DAT o un .zip / .rar de la partida.'],
    ['That PS3 save has no custom logos.', "Cette sauvegarde PS3 n'a aucun logo perso.", 'Dieser PS3-Spielstand enthält keine eigenen Logos.', 'Questo salvataggio PS3 non ha loghi personalizzati.', 'Esa partida de PS3 no tiene logotipos personalizados.'],
    ['That logo is already in your library.', 'Ce logo est déjà dans votre bibliothèque.', 'Dieses Logo ist bereits in deiner Bibliothek.', 'Questo logo è già nella tua libreria.', 'Ese logotipo ya está en tu biblioteca.'],
    ['Those logos are already in your library.', 'Ces logos sont déjà dans votre bibliothèque.', 'Diese Logos sind bereits in deiner Bibliothek.', 'Questi loghi sono già nella tua libreria.', 'Esos logotipos ya están en tu biblioteca.'],
    ['1 logo added to the library.', '1 logo ajouté à la bibliothèque.', '1 Logo zur Bibliothek hinzugefügt.', '1 logo aggiunto alla libreria.', '1 logotipo añadido a la biblioteca.'],
    ['{} logos added to the library.', '{} logos ajoutés à la bibliothèque.', '{} Logos zur Bibliothek hinzugefügt.', '{} loghi aggiunti alla libreria.', '{} logotipos añadidos a la biblioteca.'],
    ['1 logo added to the library from the PS3 save.', '1 logo ajouté à la bibliothèque depuis la sauvegarde PS3.', '1 Logo aus dem PS3-Spielstand zur Bibliothek hinzugefügt.', '1 logo aggiunto alla libreria dal salvataggio PS3.', '1 logotipo añadido a la biblioteca desde la partida de PS3.'],
    ['{} logos added to the library from the PS3 save.', '{} logos ajoutés à la bibliothèque depuis la sauvegarde PS3.', '{} Logos aus dem PS3-Spielstand zur Bibliothek hinzugefügt.', '{} loghi aggiunti alla libreria dal salvataggio PS3.', '{} logotipos añadidos a la biblioteca desde la partida de PS3.'],
    ['{} put in the game.', '{} ajouté(s) au jeu.', '{} ins Spiel übernommen.', '{} messi nel gioco.', '{} puestos en el juego.'],
    ['The game has no logo save yet: save a logo once in the Paint Tool, then pick them in the Library.', "Le jeu n'a pas encore de sauvegarde de logos : enregistrez un logo une fois dans le Paint Tool, puis choisissez-les dans la bibliothèque.", 'Das Spiel hat noch keinen Logo-Spielstand: Speichere einmal ein Logo im Paint Tool und wähle sie dann in der Bibliothek.', 'Il gioco non ha ancora un salvataggio dei loghi: salva una volta un logo nel Paint Tool, poi sceglili nella libreria.', 'El juego aún no tiene partida de logotipos: guarda un logotipo una vez en el Paint Tool y luego elígelos en la biblioteca.'],
    ['The game is full (20): pick which ones to use in the Library.', 'Le jeu est plein (20) : choisissez ceux à utiliser dans la bibliothèque.', 'Das Spiel ist voll (20): Wähle in der Bibliothek, welche verwendet werden.', 'Il gioco è pieno (20): scegli quali usare nella libreria.', 'El juego está lleno (20): elige cuáles usar en la biblioteca.'],
    ['Could not unpack {}.', 'Impossible de décompresser {}.', '{} konnte nicht entpackt werden.', 'Impossibile estrarre {}.', 'No se pudo descomprimir {}.'],
    ['Your logo save is damaged (checksum mismatch), so it was left untouched.', "Votre sauvegarde de logos est endommagée (somme de contrôle incorrecte) : elle n'a pas été modifiée.", 'Dein Logo-Spielstand ist beschädigt (Prüfsumme falsch) und wurde nicht verändert.', 'Il salvataggio dei loghi è danneggiato (checksum errato), quindi non è stato modificato.', 'Tu partida de logotipos está dañada (suma de comprobación incorrecta), así que no se ha tocado.'],
    ['The logo save could not be read: {}', 'Impossible de lire la sauvegarde de logos : {}', 'Der Logo-Spielstand konnte nicht gelesen werden: {}', 'Impossibile leggere il salvataggio dei loghi: {}', 'No se pudo leer la partida de logotipos: {}'],
    ['This is not a WWE 2K14 logo save.', "Ce n'est pas une sauvegarde de logos WWE 2K14.", 'Das ist kein WWE-2K14-Logo-Spielstand.', 'Questo non è un salvataggio dei loghi di WWE 2K14.', 'No es una partida de logotipos de WWE 2K14.'],
    ['This is not a WWE 2K14 PS3 logo file.', "Ce n'est pas un fichier de logos PS3 de WWE 2K14.", 'Das ist keine WWE-2K14-PS3-Logodatei.', 'Questo non è un file di loghi PS3 di WWE 2K14.', 'No es un archivo de logotipos de PS3 de WWE 2K14.'],
    // Button mapper
    ['Custom', 'Perso', 'Eigene', 'Personalizzati', 'Personalizado'],
    ['Choose which controller button and which keys press each button. Save, then the changes apply the next time you press Play.', "Changez l'action de chaque bouton de la manette et les touches qui appuient sur chaque bouton. Les changements s'appliquent au prochain lancement.", 'Lege fest, was jede Controllertaste tut und welche Tasten jede Taste drücken. Die Änderungen gelten ab dem nächsten Spielstart.', 'Cambia la funzione di ogni pulsante del controller e i tasti che premono ogni pulsante. Le modifiche valgono dal prossimo avvio.', 'Cambia lo que hace cada botón del mando y qué teclas pulsan cada botón. Los cambios se aplican la próxima vez que pulses Jugar.'],
    ['Reset to default', 'Rétablir par défaut', 'Zurücksetzen', 'Ripristina predefiniti', 'Restablecer'],
    ['Click a button or key, then press the new one.', 'Cliquez sur un bouton ou une touche, puis appuyez sur le nouveau.', 'Klicke auf eine Taste und drücke dann die neue.', 'Clicca su un pulsante o un tasto, poi premi quello nuovo.', 'Haz clic en un botón o una tecla y pulsa el nuevo.'],
    ['Press a key\u2026', 'Appuyez sur une touche\u2026', 'Taste drücken\u2026', 'Premi un tasto\u2026', 'Pulsa una tecla\u2026'],
    ['Press a button\u2026', 'Appuyez sur un bouton\u2026', 'Taste drücken\u2026', 'Premi un pulsante\u2026', 'Pulsa un botón\u2026'],
    ['Strike', 'Frappe', 'Schlag', 'Colpo', 'Golpe'],
    ['Pick up', 'Ramasser', 'Aufheben', 'Raccogli', 'Recoger'],
    ['Apron / ring', 'Tablier / ring', 'Ringschürze / Ring', 'Bordo ring / ring', 'Delantal / ring'],
    ['Change target', 'Changer de cible', 'Ziel wechseln', 'Cambia bersaglio', 'Cambiar de objetivo'],
    ['Change hold', 'Changer de prise', 'Griff wechseln', 'Cambia presa', 'Cambiar de agarre'],
    ['Taunt', 'Provocation', 'Provokation', 'Provocazione', 'Burla'],
    ['Export saves your scheme to a file you can keep or share.', 'Exporter enregistre votre schéma dans un fichier à garder ou partager.', 'Exportieren speichert dein Schema in einer Datei zum Aufbewahren oder Teilen.', 'Esporta salva il tuo schema in un file da conservare o condividere.', 'Exportar guarda tu esquema en un archivo para conservarlo o compartirlo.'],
    ['Save after importing.', 'Enregistrez après avoir importé.', 'Nach dem Importieren speichern.', 'Salva dopo aver importato.', 'Guarda después de importar.'],
    ['Import', 'Importer', 'Importieren', 'Importa', 'Importar'],
    ['Export', 'Exporter', 'Exportieren', 'Esporta', 'Exportar'],
    ['Control scheme exported.', 'Schéma de commandes exporté.', 'Steuerungsschema exportiert.', 'Schema dei comandi esportato.', 'Esquema de controles exportado.'],
    ['Control scheme loaded. Press Save to keep it.', 'Schéma de commandes chargé. Cliquez sur Enregistrer pour le garder.', 'Steuerungsschema geladen. Klicke auf Speichern, um es zu behalten.', 'Schema dei comandi caricato. Premi Salva per tenerlo.', 'Esquema de controles cargado. Pulsa Guardar para conservarlo.'],
    ['That file is not a control scheme.', "Ce fichier n'est pas un schéma de commandes.", 'Diese Datei ist kein Steuerungsschema.', 'Questo file non è uno schema dei comandi.', 'Ese archivo no es un esquema de controles.'],
    ['Could not save the file: {}', "Impossible d'enregistrer le fichier : {}", 'Datei konnte nicht gespeichert werden: {}', 'Impossibile salvare il file: {}', 'No se pudo guardar el archivo: {}'],
    ['Save', 'Enregistrer', 'Speichern', 'Salva', 'Guardar'],
    ['Controls saved.', 'Commandes enregistrées.', 'Steuerung gespeichert.', 'Comandi salvati.', 'Controles guardados.'],
    ['Left stick click', 'Clic du stick gauche', 'Linken Stick drücken', 'Clic levetta sinistra', 'Clic del stick izquierdo'],
    ['Left stick up', 'Stick gauche haut', 'Linker Stick hoch', 'Levetta sinistra su', 'Stick izquierdo arriba'],
    ['Left stick down', 'Stick gauche bas', 'Linker Stick runter', 'Levetta sinistra giù', 'Stick izquierdo abajo'],
    ['Left stick left', 'Stick gauche gauche', 'Linker Stick links', 'Levetta sinistra sinistra', 'Stick izquierdo izquierda'],
    ['Left stick right', 'Stick gauche droite', 'Linker Stick rechts', 'Levetta sinistra destra', 'Stick izquierdo derecha'],
    ['Right stick up', 'Stick droit haut', 'Rechter Stick hoch', 'Levetta destra su', 'Stick derecho arriba'],
    ['Right stick down', 'Stick droit bas', 'Rechter Stick runter', 'Levetta destra giù', 'Stick derecho abajo'],
    ['Right stick left', 'Stick droit gauche', 'Rechter Stick links', 'Levetta destra sinistra', 'Stick derecho izquierda'],
    ['Right stick right', 'Stick droit droite', 'Rechter Stick rechts', 'Levetta destra destra', 'Stick derecho derecha'],
    ['D-pad up', 'Croix haut', 'Steuerkreuz hoch', 'Croce su', 'Cruceta arriba'],
    ['D-pad down', 'Croix bas', 'Steuerkreuz runter', 'Croce giù', 'Cruceta abajo'],
    ['D-pad left', 'Croix gauche', 'Steuerkreuz links', 'Croce sinistra', 'Cruceta izquierda'],
    ['D-pad right', 'Croix droite', 'Steuerkreuz rechts', 'Croce destra', 'Cruceta derecha'],
];

// Paragraphs with bold words are translated whole (data-i18n-html="key").
const I18N_HTML = {
    'library-intro': {
        fr: 'Le jeu ne contient que <strong>20 logos perso</strong>. La bibliothèque les garde <strong>tous</strong>, autant que vous voulez, et vous choisissez les 20 qui vont dans le jeu.',
        de: 'Das Spiel fasst nur <strong>20 eigene Logos</strong>. Die Bibliothek bewahrt <strong>alle</strong> deine Logos auf, so viele du willst, und du wählst, welche 20 ins Spiel kommen.',
        it: 'Il gioco contiene solo <strong>20 loghi personalizzati</strong>. La libreria li conserva <strong>tutti</strong>, quanti ne vuoi, e scegli tu i 20 da mettere nel gioco.',
        es: 'El juego solo admite <strong>20 logotipos personalizados</strong>. La biblioteca guarda <strong>todos</strong> los tuyos, tantos como quieras, y tú eliges los 20 que van al juego.',
    },
    'library-list': {
        fr: '<li><strong>Ajouter des logos</strong> et <strong>Importer une sauvegarde PS3</strong> placent les logos dans la bibliothèque. Tant que le jeu a des emplacements libres, ils vont aussi directement dans le jeu.</li><li><strong>Cliquez sur un logo</strong> de la bibliothèque pour l\'ajouter au jeu ou l\'en retirer. Les logos du jeu portent une coche.</li><li>Un logo retiré du jeu reste dans la bibliothèque. La <strong>corbeille</strong> sur un logo le supprime définitivement de la bibliothèque.</li><li>Les logos créés dans le Paint Tool du jeu rejoignent la bibliothèque automatiquement.</li>',
        de: '<li><strong>Logos hinzufügen</strong> und <strong>PS3-Spielstand importieren</strong> legen Logos in der Bibliothek ab. Solange das Spiel freie Plätze hat, kommen sie auch direkt ins Spiel.</li><li><strong>Klicke auf ein Logo</strong> in der Bibliothek, um es ins Spiel zu übernehmen oder herauszunehmen. Logos im Spiel haben einen Haken.</li><li>Ein aus dem Spiel genommenes Logo bleibt in der Bibliothek. Der <strong>Papierkorb</strong> an einem Logo löscht es endgültig aus der Bibliothek.</li><li>Logos, die du im Paint Tool des Spiels erstellst, landen automatisch in der Bibliothek.</li>',
        it: '<li><strong>Aggiungi loghi</strong> e <strong>Importa salvataggio PS3</strong> mettono i loghi nella libreria. Finché il gioco ha spazi liberi, vanno anche direttamente nel gioco.</li><li><strong>Clicca su un logo</strong> nella libreria per metterlo nel gioco o toglierlo. I loghi nel gioco hanno una spunta.</li><li>Un logo tolto dal gioco resta nella libreria. Il <strong>cestino</strong> su un logo lo elimina definitivamente dalla libreria.</li><li>I loghi creati nel Paint Tool del gioco entrano nella libreria da soli.</li>',
        es: '<li><strong>Añadir logotipos</strong> e <strong>Importar partida de PS3</strong> guardan los logotipos en la biblioteca. Mientras el juego tenga huecos libres, también van directamente al juego.</li><li><strong>Haz clic en un logotipo</strong> de la biblioteca para ponerlo en el juego o quitarlo. Los logotipos del juego llevan una marca.</li><li>Un logotipo quitado del juego se queda en la biblioteca. La <strong>papelera</strong> de un logotipo lo elimina de la biblioteca para siempre.</li><li>Los logotipos que crees en el Paint Tool del juego se añaden solos a la biblioteca.</li>',
    },
};

const I18N_INDEX = { fr: 1, de: 2, it: 3, es: 4 };
let i18nLang = 'en';
const i18nExact = {};
const i18nPatterns = {};
for (const code of Object.keys(I18N_INDEX)) {
    i18nExact[code] = new Map();
    i18nPatterns[code] = [];
}
for (const row of I18N_ROWS) {
    for (const [code, index] of Object.entries(I18N_INDEX)) {
        const english = row[0];
        const translated = row[index];
        if (!translated) continue;
        if (english.includes('{}')) {
            const source = '^' + english.split('{}').map(part => part.replace(/[.*+?^$()|[\]\\]/g, '\\$&')).join('(.+?)') + '$';
            i18nPatterns[code].push([new RegExp(source), translated]);
        } else {
            i18nExact[code].set(english, translated);
        }
    }
}

function i18nFill(template, values) {
    let i = 0;
    return template.replace(/\{\}/g, () => (i < values.length ? values[i++] : ''));
}

function i18nLookup(text) {
    const exact = i18nExact[i18nLang].get(text);
    if (exact !== undefined) return exact;
    for (const [pattern, translated] of i18nPatterns[i18nLang]) {
        const match = text.match(pattern);
        if (match) return i18nFill(translated, match.slice(1));
    }
    return undefined;
}

// Translates an English text; with extra arguments the key is a template: t('Unlocked {}', date).
function t(text, ...values) {
    if (values.length) text = i18nFill(text, values.map(String));
    if (i18nLang === 'en' || !text) return text;
    const normal = text.replace(/\s+/g, ' ').trim();
    const whole = i18nLookup(normal);
    if (whole !== undefined) return whole;
    // Messages built from several sentences: translate each one.
    const sentences = normal.match(/[^.!?]+[.!?]+(?=\s|$)|[^.!?]+$/g);
    if (!sentences || sentences.length < 2) return text;
    let changed = false;
    const out = sentences.map(sentence => {
        const s = sentence.trim();
        const tr = i18nLookup(s);
        if (tr !== undefined) changed = true;
        return tr ?? s;
    });
    return changed ? out.join(' ') : text;
}

function launcherLocale() {
    return I18N_LOCALES[i18nLang] || 'en-US';
}

// --- Translating the page -------------------------------------------------------------------------------------------

const i18nOriginal = new WeakMap();   // text node -> English text
const i18nShown = new WeakMap();      // text node -> text we put there
const I18N_ATTRIBUTES = ['title', 'aria-label', 'placeholder', 'alt'];

function i18nSkip(node) {
    const element = node.nodeType === 1 ? node : node.parentElement;
    return !element || element.closest('script, style, [data-no-i18n]');
}

function i18nText(node) {
    if (i18nSkip(node)) return;
    const current = node.nodeValue;
    let english = i18nOriginal.get(node);
    if (english === undefined || current !== i18nShown.get(node)) {
        english = current;
        i18nOriginal.set(node, english);
    }
    if (!/[A-Za-z]{2}/.test(english)) return;
    const core = english.trim();
    const translated = i18nLang === 'en' ? core : t(core);
    const out = english.replace(core, translated);
    i18nShown.set(node, out);
    if (out !== current) node.nodeValue = out;
}

function i18nAttributes(element) {
    if (i18nSkip(element)) return;
    for (const name of I18N_ATTRIBUTES) {
        if (!element.hasAttribute(name)) continue;
        const key = `i18n${name.replace(/-./g, m => m[1].toUpperCase())}`;
        const current = element.getAttribute(name);
        if (element.dataset[key] === undefined || current !== element.dataset[`${key}Shown`]) element.dataset[key] = current;
        const translated = i18nLang === 'en' ? element.dataset[key] : t(element.dataset[key]);
        element.dataset[`${key}Shown`] = translated;
        if (translated !== current) element.setAttribute(name, translated);
    }
}

const i18nHtmlOriginal = new WeakMap();
function i18nHtml(element) {
    if (!i18nHtmlOriginal.has(element)) i18nHtmlOriginal.set(element, element.innerHTML);
    const html = i18nLang === 'en' ? null : I18N_HTML[element.dataset.i18nHtml]?.[i18nLang];
    element.innerHTML = html ?? i18nHtmlOriginal.get(element);
}

function i18nTree(root) {
    if (root.nodeType === 3) { i18nText(root); return; }
    if (root.nodeType !== 1) return;
    if (root.matches('[data-i18n-html]')) { i18nHtml(root); return; }
    i18nAttributes(root);
    const walker = document.createTreeWalker(root, NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT);
    let node = walker.nextNode();
    while (node) {
        if (node.nodeType === 3) {
            i18nText(node);
            node = walker.nextNode();
        } else if (node.matches('[data-i18n-html]')) {
            i18nHtml(node);
            node = walker.nextSibling() || i18nNextOutside(walker);
        } else {
            i18nAttributes(node);
            node = walker.nextNode();
        }
    }
}

function i18nNextOutside(walker) {
    while (walker.parentNode()) {
        const next = walker.nextSibling();
        if (next) return next;
    }
    return null;
}

let i18nBusy = false;
const i18nObserver = new MutationObserver(mutations => {
    if (i18nBusy || i18nLang === 'en') return;
    i18nBusy = true;
    for (const m of mutations) {
        if (m.type === 'characterData') i18nText(m.target);
        else if (m.type === 'attributes') i18nAttributes(m.target);
        else m.addedNodes.forEach(n => {
            if (n.parentElement?.closest('[data-i18n-html]')) return;
            i18nTree(n);
        });
    }
    i18nBusy = false;
});
i18nObserver.observe(document.body, {
    subtree: true, childList: true, characterData: true, attributes: true, attributeFilter: I18N_ATTRIBUTES,
});

// Switches the launcher's language; `language` is the game setting value (english, french, ...).
function setLauncherLanguage(language) {
    const code = I18N_LANGUAGES[(language || 'english').toLowerCase()] || 'en';
    if (code === i18nLang) return false;
    i18nLang = code;
    document.documentElement.lang = code;
    i18nBusy = true;
    i18nTree(document.body);
    i18nBusy = false;
    return true;
}
