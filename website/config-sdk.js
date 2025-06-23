import { initializeApp } from "https://www.gstatic.com/firebasejs/11.4.0/firebase-app.js";
import { getAnalytics } from "https://www.gstatic.com/firebasejs/11.4.0/firebase-analytics.js";

const firebaseConfig = {
  apiKey: "AIzaSyDX9FQQai2rWeUubCX903z-yPMro_TGTIQ",
  authDomain: "projectv1-a9190.firebaseapp.com",
  databaseURL: "https://projectv1-a9190-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "projectv1-a9190",
  storageBucket: "projectv1-a9190.firebasestorage.app",
  messagingSenderId: "343955967254",
  appId: "1:343955967254:web:6de9646cb6140a07a89437",
  measurementId: "G-N903P4FH5W"
};

const app = initializeApp(firebaseConfig);

let analytics;
try {
  analytics = getAnalytics(app);
  console.log('Firebase Analytics initialized successfully');
} catch (error) {
  console.warn('Analytics could not be initialized:', error.message);
}

export { app, analytics };
