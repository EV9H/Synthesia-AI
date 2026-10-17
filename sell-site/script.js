(function () {
  const site = window.SITE || {};
  const name = site.productName || "Harmonia";

  // Apply brand + price from config.js
  document.querySelectorAll("[data-brand]").forEach((el) => {
    el.textContent = name;
  });
  document.querySelectorAll("[data-price]").forEach((el) => {
    el.textContent = site.price || "$49";
  });
  document.querySelectorAll("[data-price-note]").forEach((el) => {
    el.textContent = site.priceNote || "one-time · own it forever";
  });

  const title = document.querySelector("title");
  if (title) title.textContent = name + " — AI MIDI for your sounds";

  // Checkout links
  const checkoutUrl = site.lemonCheckoutUrl || "#";
  document.querySelectorAll("[data-checkout]").forEach((a) => {
    a.setAttribute("href", checkoutUrl);
    if (checkoutUrl === "#" || checkoutUrl === "") {
      a.addEventListener("click", (e) => {
        e.preventDefault();
        alert(
          "Checkout URL not set yet.\n\n" +
            "1. Create the product in Lemon Squeezy\n" +
            "2. Copy the checkout link\n" +
            "3. Paste it into config.js → lemonCheckoutUrl\n\n" +
            "See SETUP.md."
        );
      });
    }
  });

  // Support email (placeholder until TODO in config.js is replaced)
  const support = document.getElementById("supportLink");
  if (support && site.supportEmail) {
    support.setAttribute("href", "mailto:" + site.supportEmail);
    support.textContent = site.supportEmail;
  }

  // Genre pills
  const pills = document.getElementById("genrePills");
  if (pills && Array.isArray(site.genres)) {
    pills.innerHTML = site.genres
      .map((g) => '<span class="pill">' + g + "</span>")
      .join("");
  }

  // Year
  const year = document.getElementById("year");
  if (year) year.textContent = String(new Date().getFullYear());

  // Mobile nav
  const toggle = document.getElementById("navToggle");
  const links = document.getElementById("navLinks");
  if (toggle && links) {
    toggle.addEventListener("click", () => {
      const open = links.classList.toggle("is-open");
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
    links.querySelectorAll("a").forEach((a) => {
      a.addEventListener("click", () => {
        links.classList.remove("is-open");
        toggle.setAttribute("aria-expanded", "false");
      });
    });
  }

  // Highlight playing demo card
  document.querySelectorAll(".demo-card audio").forEach((audio) => {
    const card = audio.closest(".demo-card");
    audio.addEventListener("play", () => {
      document.querySelectorAll(".demo-card").forEach((c) => c.classList.remove("is-playing"));
      if (card) card.classList.add("is-playing");
      document.querySelectorAll(".demo-card audio").forEach((other) => {
        if (other !== audio) other.pause();
      });
    });
    audio.addEventListener("pause", () => {
      if (card && audio.paused) card.classList.remove("is-playing");
    });
  });
})();
