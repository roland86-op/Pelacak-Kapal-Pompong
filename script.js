console.log("Script Loaded");
console.log(L);

//=============================
// Leaflet Map
//=============================

console.log(document.getElementById("map"));
var map = L.map('map').setView([0,0],15);

L.tileLayer(
'https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',
{
    maxZoom:19,
    attribution:'© OpenStreetMap'
}).addTo(map);

//=============================
// Marker Icon
//=============================

var greenIcon = new L.Icon({
    iconUrl:'marker-icon-green.png',
    shadowUrl:'marker-shadow.png',
    iconSize:[25,41],
    iconAnchor:[12,41]
});

var yellowIcon = new L.Icon({
    iconUrl:'marker-icon-yellow.png',
    shadowUrl:'marker-shadow.png',
    iconSize:[25,41],
    iconAnchor:[12,41]
});

var redIcon = new L.Icon({
    iconUrl:'marker-icon-red.png',
    shadowUrl:'marker-shadow.png',
    iconSize:[25,41],
    iconAnchor:[12,41]
});

//=============================
// Marker
//=============================

var marker = L.marker([0,0],{
    icon:greenIcon
}).addTo(map);

//=============================
// Polyline
//=============================

var path = [];

var polyline = L.polyline(path,{
    color:'blue',
    weight:4
}).addTo(map);

//=============================
// Update Data
//=============================

let updating = false;
let lastPacket = "";

async function updateData()
{
    if (updating) return;
    updating = true;

    try
    {
        const response = await fetch('/data?_=' + Date.now(), {
            cache: "no-store"
        });

        const data = await response.json();

        // Ambil data date dan time
        const dateVal = data.date || "--/--/----";
        const timeVal = data.time || "--:--:--";
        const rollVal = data.roll !== undefined ? data.roll : "-";
        const pitchVal = data.pitch !== undefined ? data.pitch : "-";
        const satVal = data.sat !== undefined ? data.sat : "-";

        // Logika Status Sinyal LoRa berdasarkan RSSI (misal RSSI == 0 berarti putus)
        let statusDisplay = data.status;
        if (data.rssi === 0 || data.rssi === "0") {
            statusDisplay = "Sinyal LoRa Putus";
        }

        // String pembanding untuk mengecek perubahan data
        const packet = `${dateVal}|${timeVal}|${data.lat}|${data.lon}|${data.speed}|${rollVal}|${pitchVal}|${satVal}|${data.rssi}|${statusDisplay}`;

        if (packet !== lastPacket)
        {
            lastPacket = packet;

            // Update elemen teks UI
            if (document.getElementById("lat")) document.getElementById("lat").innerHTML = Number(data.lat).toFixed(6);
            if (document.getElementById("lon")) document.getElementById("lon").innerHTML = Number(data.lon).toFixed(6);
            if (document.getElementById("speed")) document.getElementById("speed").innerHTML = data.speed;
            if (document.getElementById("status")) document.getElementById("status").innerHTML = statusDisplay;
            if (document.getElementById("rssi")) document.getElementById("rssi").innerHTML = data.rssi;
            if (document.getElementById("roll")) document.getElementById("roll").innerHTML = rollVal;
            if (document.getElementById("pitch")) document.getElementById("pitch").innerHTML = pitchVal;
            if (document.getElementById("sat")) document.getElementById("sat").innerHTML = satVal;

            // Ubah warna marker sesuai status koneksi
            if (statusDisplay === "Sinyal LoRa Putus") {
                marker.setIcon(redIcon);
            } else {
                marker.setIcon(greenIcon);
            }

            // Update peta
            if (typeof marker !== 'undefined' && typeof map !== 'undefined') {
                marker.setLatLng([data.lat, data.lon]);
                map.setView([data.lat, data.lon]);

                path.push([data.lat, data.lon]);
                polyline.setLatLngs(path);
            }

            // Update tabel riwayat
            const tbody = document.querySelector("#history tbody");
            if (tbody) {
                const row = tbody.insertRow(0);

                row.insertCell(0).innerHTML = dateVal;
                row.insertCell(1).innerHTML = timeVal;
                row.insertCell(2).innerHTML = Number(data.lat).toFixed(6);
                row.insertCell(3).innerHTML = Number(data.lon).toFixed(6);
                row.insertCell(4).innerHTML = data.speed;
                row.insertCell(5).innerHTML = rollVal;
                row.insertCell(6).innerHTML = pitchVal;
                row.insertCell(7).innerHTML = satVal;
                row.insertCell(8).innerHTML = data.rssi;
                row.insertCell(9).innerHTML = statusDisplay;

                while (tbody.rows.length > 20)
                {
                    tbody.deleteRow(20);
                }
            }
        }
    }
    catch(err)
    {
        console.error(err);
    }
    finally
    {
        updating = false;
    }
}

//=============================
// Refresh
//=============================

updateData();

setInterval(updateData, 5000);