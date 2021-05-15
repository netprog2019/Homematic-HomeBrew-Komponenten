
# Homematic-HomeBrew-Komponenten

<br>

| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Es handelt es sich hierbei um ein reines Hobby-Projekt - Die Verwendung erfolgt auf eigene Gefahr!&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; |
|:---:|

<br>

## Was ist das?

- Ich stelle hier, verschiedene Selbstbaugeräte unter Verwendung der AskSinPP Library zu Verfügung, welche ich derzeit in meiner HomeMatic-Umgebung selbst im Einsatz habe.

- Sowie einen Stripdown von Jérômes [JP-HB-Devices-addon](https://github.com/jp112sdl/JP-HB-Devices-addon) zur Unterstützung folgender Projkte/Geräte.
	- [HB-UNI-Sen-CAP-MOIST von Jérôme/jp112sdl](https://github.com/jp112sdl/HB-UNI-Sen-CAP-MOIST)
	- [HB-UNI-RGB-LED-CTRL von Jérôme/jp112sdl](https://github.com/jp112sdl/HB-UNI-RGB-LED-CTRL)
	- [HB-UNI-Sen-WEA von Jérôme/jp112sdl](https://github.com/jp112sdl/HB-UNI-Sen-WEA/wiki)
	- Ich habe dazu, das notwendigste aus Jérômes [JP-HB-Devices-addon](https://github.com/jp112sdl/JP-HB-Devices-addon) "herausoperiert" und ein paar Modifikationen vollzogen, um ein separates AddOn, nur für die genannten Geräte zu haben.<br><br>


## Warum?

Auf Grund der Inspirationen und Vorarbeit von [TomMajor](https://github.com/TomMajor/SmartHome/tree/master/HB-TM-JP-AddOn-Reduced), kam auch ich zu dem Entschluss,<br>mir ein eigenes AddOn zu erstellen.<br>

Ebenso wie [TomMajor](https://github.com/TomMajor/SmartHome) habe ich natürlich, vollstes Vertrauen und Respekt in Jérômes Modifikationen und Patches<br>für seine vielen Selbstbau-Projekte!<br>

Da ich zur Zeit einerseits, aber nur wenige davon, aktiv einsetze und ich anderseits, gern das Gefühl hätte,<br>noch ungefähr zu wissen, was man das AddOn tut. Sowie unabhängig von Jérômes AddOn sein möchte,<br> falls er mal den Support für neuere Versionen der RM/CCU Firmware einstellen *sollte*, entstand somit ebenso<br>ein Stripdown des JP-HB-Devices-addon, eben nur für die von mir genutzen Geräte.

**Das alles, wäre selbstverständlich ohne die hervorragende Arbeit von [pa-pa](https://github.com/pa-pa/AskSinPP),  [Jérôme](https://github.com/jp112sdl/Beispiel_AskSinPP),  [TomMajor](https://github.com/TomMajor/SmartHome) und vielen anderen nicht möglich gewesen.**

<br><br>

# [CCU AddOn](https://github.com/netprog2019/Homematic-HomeBrew-Komponenten/tree/main/HB-MR-Devices-AddOn)

|  | Version | Status | letzte getestete Firmware (RaspberryMatic) |
|:---:|:---:|:---:|:---:|
| :heavy_check_mark: | 1.10 | lauffähig unter CCU / RaspberryMatic | 3.57.5.20210424 |


<br><br><br>
## <img src="HB-MR-Devices-AddOn/src/addon/www/config/img/devices/250/hb-moist-sens.png" width=100/> &nbsp;&nbsp; Bodenfeuchtesensor 
#### [Relaunch des HB-UNI-Sen-CAP-MOIST von Jérôme/jp112sdl](https://github.com/jp112sdl/HB-UNI-Sen-CAP-MOIST)
:arrow_right: [Projektseite HB-MR-MOIST-SENS](https://github.com/netprog2019/Homematic-HomeBrew-Komponenten/tree/main/HB-MR-MOIST-SENS)

- kapazitiver Bodenfeuchtesensor für bis zu 7 angeschlossene Sensoren 



<br><br><br>
## <img src="HB-MR-Devices-AddOn/src/addon/www/config/img/devices/250/hb-moist-sens-sw2.png" width=100/> &nbsp;&nbsp; Bodenfeuchtesensor mit zwei zusätzlichen Schaltausgängen 
:arrow_right: [Projektseite HB-MR-MOIST-SENS-SW2](https://github.com/netprog2019/Homematic-HomeBrew-Komponenten/tree/main/HB-MR-MOIST-SENS-SW2)

- kapazitiver Bodenfeuchtesensor, mit SabotageMsg bei zu großer Messdifferenz
- Niederspannungs-Schaltausgang 1 - (z.Bsp. zur Ansteuerung einer Bewässerungspumpe)  
- Niederspannungs-Schaltausgang 2 - (z.Bsp. zur Ansteuerung einer LED Beleuchtung)



<br><br><br>
## <img src="HB-MR-Devices-AddOn/src/addon/www/config/img/devices/250/hb-uni-rgb-led-ctrl.png" width=100/> &nbsp;&nbsp; Homematic kompatibler LED Controller 
#### [Relaunch des HB-UNI-RGB-LED-CTRL von Jérôme/jp112sdl](https://github.com/jp112sdl/HB-UNI-RGB-LED-CTRL)
:arrow_right: [Projektseite HB-MR-MOIST-SENS](https://github.com/netprog2019/Homematic-HomeBrew-Komponenten/tree/main/HB-MR-MOIST-SENS)

- für adressierbare RGB LEDs (WS28xx, Neopixel,...)



<br><br><br>
## <img src="HB-MR-Devices-AddOn/src/addon/www/config/img/devices/250/hb-wea-sens.png" width=100/> &nbsp;&nbsp; Homematic Wetterstation 
#### [Relaunch der HB-UNI-Sen-WEA von Jérôme/jp112sdl](https://github.com/jp112sdl/HB-UNI-Sen-WEA/wiki)
:arrow_right: [Projektseite HB-MR-MOIST-SENS](https://github.com/netprog2019/Homematic-HomeBrew-Komponenten/tree/main/HB-MR-MOIST-SENS)

#### Die Wetterstation liefert folgende Messwerte:
- Temperatur
- Luftfeuchtigkeit
- Luftdruck
- Helligkeit
- Regenerkennung
- Regenmenge
- Windgeschwindigkeit
- Böen-Geschwindigkeit
- Windrichtung
- Windrichtung-Schwankungsbreite
- UV-Index
- Blitz-Anzahl und -Entfernung



<br><br><br>
## <img src="HB-MR-Devices-AddOn/src/addon/www/config/img/devices/250/hb-sc-sd-thp.png" width=100/> &nbsp;&nbsp; Garagensensor mit Türkontakt, Rauchmelder & div. Klimadaten
:arrow_right: [Projektseite HB-MR-SC-SD-THP](https://github.com/netprog2019/Homematic-HomeBrew-Komponenten/tree/main/HB-MR-SC-SD-THP)

- ShutterContact Schnittstelle zur Überwachung des Garagentores mittels Reed-Kontakt
- SmokeDetektor zur Brandüberwachung mittels MQ-135 Sensor
- Übertragung von Temperatur, Luftfeuchte und Luftdruck mittels BME-280 Sensor



<br><br><br>
## Lizenz

**Creative Commons BY-NC-SA**<br>
Give Credit, NonCommercial, ShareAlike

<a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by-nc-sa/4.0/88x31.png" /></a><br />This work is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/">Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License</a>.


[comment]: # (:large_blue_circle:)
