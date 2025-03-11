// Import the functions you need from the SDKs you need
import { initializeApp } from "https://www.gstatic.com/firebasejs/11.4.0/firebase-app.js";
import { getAnalytics } from "https://www.gstatic.com/firebasejs/11.4.0/firebase-analytics.js";
import { getDatabase, ref, set, onValue} from "https://www.gstatic.com/firebasejs/11.4.0/firebase-database.js";
// TODO: Add SDKs for Firebase products that you want to use
// https://firebase.google.com/docs/web/setup#available-libraries

// Your web app's Firebase configuration
// For Firebase JS SDK v7.20.0 and later, measurementId is optional
const firebaseConfig = {
  apiKey: "[MASUKKAN_API_KEY_ANDA_DI_SINI]",
  authDomain: "[MASUKKAN_AUTH_DOMAIN_ANDA_DI_SINI]",
  databaseURL: "[MASUKKAN_DATABASE_URL_ANDA_DI_SINI]",
  projectId: "[MASUKKAN_PROJECT_ID_ANDA_DI_SINI]",
  storageBucket: "[MASUKKAN_STORAGE_BUCKET_ANDA_DI_SINI]",
  messagingSenderId: "[MASUKKAN_MESSAGING_SENDER_ID_ANDA_DI_SINI]",
  appId: "[MASUKKAN_APP_ID_ANDA_DI_SINI]",
  measurementId: "[MASUKKAN_MEASUREMENT_ID_ANDA_DI_SINI]"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
const analytics = getAnalytics(app);

export {app}
